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

        const util::MATERIAL_TEXTURE_SLOT_DEBUG *findMaterialTextureSlot(const util::SUBSET_DEBUG *subset,
                                                                         const uint16_t slotType) noexcept
        {
            if (subset == nullptr)
                return nullptr;
            for (const auto &slot : subset->materialTextureSlots)
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
        return onSetPhysicsFromTableLuaToLineMesh(lua,&meshDebug->mesh.infoPhysics,nullptr);
    }

    int onGetPhysicsMeshDebugLua(lua_State *lua)
    {
        MESH_DEBUG_LUA *meshDebug = getMeshDebugFromRawTable(lua, 1, 1);
        lua_settop(lua,0);
        lua_newtable(lua); // array
        const unsigned int sCube = static_cast<unsigned int>(meshDebug->mesh.infoPhysics.lsCube.size());
        const unsigned int sTria = static_cast<unsigned int>(meshDebug->mesh.infoPhysics.lsTriangle.size());
        const unsigned int sSphe = static_cast<unsigned int>(meshDebug->mesh.infoPhysics.lsSphere.size());
        const unsigned int sComp = static_cast<unsigned int>(meshDebug->mesh.infoPhysics.lsCubeComplex.size());
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
        const unsigned int  mode_draw = util::get_mode_draw_from_string(s_mode_draw);
        if (mode_draw == std::numeric_limits<unsigned int>::max())
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
        const unsigned int  mode_cull_face = util::get_mode_cull_face_from_string(s_mode_cull_face);
        if (mode_cull_face == std::numeric_limits<unsigned int>::max())
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
        const unsigned int  mode_front_face = util::get_mode_front_face_direction_from_string(s_mode_front_face);
        if (mode_front_face == std::numeric_limits<unsigned int>::max())
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
		const char *  mode_draw   = util::get_mode_draw_from_uint(meshDebug->mesh.info_mode.mode_draw,"nil");
        lua_pushstring(lua,mode_draw);
        return 1;
    }

	int onGetMode_CullFaceMeshDebugLua(lua_State *lua)
	{
        MESH_DEBUG_LUA *meshDebug   = getMeshDebugFromRawTable(lua, 1, 1);
		const char *  mode_cull_face = util::get_mode_cull_face_from_uint(meshDebug->mesh.info_mode.mode_cull_face,"nil");
        lua_pushstring(lua,mode_cull_face);
        return 1;
    }

	int onGetMode_FrontFaceMeshDebugLua(lua_State *lua)
	{
        MESH_DEBUG_LUA *meshDebug     = getMeshDebugFromRawTable(lua, 1, 1);
		const char *  mode_front_face = util::get_mode_front_face_direction_from_uint(meshDebug->mesh.info_mode.mode_front_face_direction,"nil");
        lua_pushstring(lua,mode_front_face);
        return 1;
    }

    int onGetVersionMeshDebugLua(lua_State *lua)
    {
        MESH_DEBUG_LUA *meshDebug = getMeshDebugFromRawTable(lua, 1, 1);
        lua_pushinteger(lua, meshDebug->mesh.headerMain.version);
        return 1;
    }

    int onGetAngleMeshDebugLua(lua_State *lua)
    {
        MESH_DEBUG_LUA *meshDebug = getMeshDebugFromRawTable(lua, 1, 1);
        lua_newtable(lua);
        lua_pushnumber(lua, meshDebug->mesh.headerMesh.angleX);
        lua_setfield(lua, -2, "x");
        lua_pushnumber(lua, meshDebug->mesh.headerMesh.angleY);
        lua_setfield(lua, -2, "y");
        lua_pushnumber(lua, meshDebug->mesh.headerMesh.angleZ);
        lua_setfield(lua, -2, "z");
        return 1;
    }

    int onSetAngleMeshDebugLua(lua_State *lua)
    {
        MESH_DEBUG_LUA *meshDebug = getMeshDebugFromRawTable(lua, 1, 1);
        const float x = static_cast<float>(luaL_checknumber(lua, 2));
        const float y = static_cast<float>(luaL_checknumber(lua, 3));
        const float z = static_cast<float>(luaL_checknumber(lua, 4));
        meshDebug->mesh.headerMesh.angleX = x;
        meshDebug->mesh.headerMesh.angleY = y;
        meshDebug->mesh.headerMesh.angleZ = z;
        meshDebug->mesh.angleDefault       = VEC3(x, y, z);
        return 0;
    }

    int onGetPositionMeshDebugLua(lua_State *lua)
    {
        MESH_DEBUG_LUA *meshDebug = getMeshDebugFromRawTable(lua, 1, 1);
        lua_newtable(lua);
        lua_pushnumber(lua, meshDebug->mesh.headerMesh.posX);
        lua_setfield(lua, -2, "x");
        lua_pushnumber(lua, meshDebug->mesh.headerMesh.posY);
        lua_setfield(lua, -2, "y");
        lua_pushnumber(lua, meshDebug->mesh.headerMesh.posZ);
        lua_setfield(lua, -2, "z");
        return 1;
    }

    int onSetPositionMeshDebugLua(lua_State *lua)
    {
        MESH_DEBUG_LUA *meshDebug = getMeshDebugFromRawTable(lua, 1, 1);
        const float x = static_cast<float>(luaL_checknumber(lua, 2));
        const float y = static_cast<float>(luaL_checknumber(lua, 3));
        const float z = static_cast<float>(luaL_checknumber(lua, 4));
        meshDebug->mesh.headerMesh.posX = x;
        meshDebug->mesh.headerMesh.posY = y;
        meshDebug->mesh.headerMesh.posZ = z;
        meshDebug->mesh.positionOffset  = VEC3(x, y, z);
        return 0;
    }

    int onGetMaterialMeshDebugLua(lua_State *lua)
    {
        MESH_DEBUG_LUA *meshDebug = getMeshDebugFromRawTable(lua, 1, 1);
        const util::MATERIAL &m = meshDebug->mesh.headerMesh.material;
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
        util::MATERIAL &m = meshDebug->mesh.headerMesh.material;
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
        lua_pushinteger(lua, static_cast<lua_Integer>(meshDebug->mesh.buffer.size()));
        return 1;
    }

    int onGetTotalSubsetMeshDebugLua(lua_State *lua)
    {
        MESH_DEBUG_LUA *   meshDebug  = getMeshDebugFromRawTable(lua, 1, 1);
        const auto indexFrame = (unsigned int)luaL_checkinteger(lua, 2) - 1;
        if (indexFrame < (const unsigned int)meshDebug->mesh.buffer.size())
        {
            util::BUFFER_MESH_DEBUG *buffer = meshDebug->mesh.buffer[indexFrame];
            lua_pushinteger(lua, static_cast<lua_Integer>(buffer->subset.size()));
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
                indexFrame < meshDebug->mesh.buffer.size() ? static_cast<int>(meshDebug->mesh.buffer[indexFrame]->subset.size()) : 0;
            return lua_error_debug(lua, "\nOut of bound[indexFrame(total %d),indexSubset(total %d)\n"
                            "indexFrame %d indexSubset %d",
                       static_cast<int>(meshDebug->mesh.buffer.size()), tSubset, indexFrame + 1, indexSubset + 1);
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
                indexFrame < meshDebug->mesh.buffer.size() ? static_cast<int>(meshDebug->mesh.buffer[indexFrame]->subset.size()) : 0;
            return lua_error_debug(lua, "\nOut of bound[indexFrame(total %d),indexSubset(total %d)\n"
                            "indexFrame %d indexSubset %d",
                       static_cast<int>(meshDebug->mesh.buffer.size()), tSubset, indexFrame + 1, indexSubset + 1);
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
            const int tSubset =
                indexFrame < meshDebug->mesh.buffer.size() ? static_cast<int>(meshDebug->mesh.buffer[indexFrame]->subset.size()) : 0;
            return lua_error_debug(lua, "\nOut of bound[indexFrame(total %d),indexSubset(total %d)\n"
                            "indexFrame %d indexSubset %d",
                       static_cast<int>(meshDebug->mesh.buffer.size()), tSubset, indexFrame, indexSubset);
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
            const int tSubset =
                indexFrame < meshDebug->mesh.buffer.size() ? static_cast<int>(meshDebug->mesh.buffer[indexFrame]->subset.size()) : 0;
            return lua_error_debug(lua, "\nOut of bound[indexFrame(total %d),indexSubset(total %d)\n"
                            "indexFrame %d indexSubset %d",
                       static_cast<int>(meshDebug->mesh.buffer.size()), tSubset, indexFrame + 1, indexSubset + 1);
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
                        buffer = meshDebug->mesh.buffer[indexFrame]; // re-fetch after addVertex (may reallocate)
                        auto *             pPosition  = reinterpret_cast<VEC3 *>(buffer->position);
                        auto *             pNormal    = reinterpret_cast<VEC3 *>(buffer->normal);
                        auto *             pUv        = reinterpret_cast<VEC2 *>(buffer->uv);
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
                    buffer = meshDebug->mesh.buffer[indexFrame]; // re-fetch after addVertex (may reallocate)
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
            const int tSubset =
                indexFrame < meshDebug->mesh.buffer.size() ? static_cast<int>(meshDebug->mesh.buffer[indexFrame]->subset.size()) : 0;
            return lua_error_debug(lua, "\nOut of bound[indexFrame(total %d),indexSubset(total %d)\n"
                            "indexFrame %d indexSubset %d",
                       static_cast<int>(meshDebug->mesh.buffer.size()), tSubset, indexFrame+1, indexSubset+1);
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
                indexFrame < meshDebug->mesh.buffer.size() ? static_cast<int>(meshDebug->mesh.buffer[indexFrame]->subset.size()) : 0;
            return lua_error_debug(lua, "\nOut of bound[indexFrame(total %d),indexSubset(total %d)\n"
                            "indexFrame %d indexSubset %d",
                       static_cast<int>(meshDebug->mesh.buffer.size()), tSubset, indexFrame + 1, indexSubset + 1);
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
                indexFrame < meshDebug->mesh.buffer.size() ? static_cast<int>(meshDebug->mesh.buffer[indexFrame]->subset.size()) : 0;
            return lua_error_debug(lua, "\nOut of bound[indexFrame(total %d),indexSubset(total %d)\n"
                            "indexFrame %d indexSubset %d",
                       static_cast<int>(meshDebug->mesh.buffer.size()), tSubset, indexFrame +1, indexSubset +1);
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
                indexFrame < meshDebug->mesh.buffer.size() ? static_cast<int>(meshDebug->mesh.buffer[indexFrame]->subset.size()) : 0;
            return lua_error_debug(lua, "\nOut of bound[indexFrame(total %d),indexSubset(total %d)\n"
                            "indexFrame %d indexSubset %d",
                       static_cast<int>(meshDebug->mesh.buffer.size()), tSubset, indexFrame + 1, indexSubset + 1);
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
        if (indexFrame < static_cast<unsigned int>(meshDebug->mesh.buffer.size()) &&
            indexSubset < static_cast<unsigned int>(meshDebug->mesh.buffer[indexFrame]->subset.size()))
        {
            util::BUFFER_MESH_DEBUG *buffer = meshDebug->mesh.buffer[indexFrame];
            const util::SUBSET_DEBUG *subset = buffer->subset[indexSubset];
            const auto *slot = findMaterialTextureSlot(subset, slotType);
            if (slot && slot->texture.size())
                lua_pushstring(lua, slot->texture.c_str());
            else
                lua_pushnil(lua);
            return 1;
        }
        const int tSubset =
            indexFrame < meshDebug->mesh.buffer.size() ? static_cast<int>(meshDebug->mesh.buffer[indexFrame]->subset.size()) : 0;
        return lua_error_debug(lua, "\nOut of bound[indexFrame(total %d),indexSubset(total %d)\n"
                        "indexFrame %d indexSubset %d",
                   static_cast<int>(meshDebug->mesh.buffer.size()), tSubset, indexFrame + 1, indexSubset + 1);
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
        if (indexFrame < static_cast<unsigned int>(meshDebug->mesh.buffer.size()) &&
            indexSubset < static_cast<unsigned int>(meshDebug->mesh.buffer[indexFrame]->subset.size()))
        {
            util::BUFFER_MESH_DEBUG *buffer = meshDebug->mesh.buffer[indexFrame];
            util::SUBSET_DEBUG *subset = buffer->subset[indexSubset];
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
        const int tSubset =
            indexFrame < meshDebug->mesh.buffer.size() ? static_cast<int>(meshDebug->mesh.buffer[indexFrame]->subset.size()) : 0;
        return lua_error_debug(lua, "\nOut of bound[indexFrame(total %d),indexSubset(total %d)\n"
                        "indexFrame %d indexSubset %d",
                   static_cast<int>(meshDebug->mesh.buffer.size()), tSubset, indexFrame + 1, indexSubset + 1);
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
        const unsigned int indexFrame = top > 1 ? (unsigned int)luaL_checkinteger(lua, 2) - 1 : static_cast<unsigned int>(meshDebug->mesh.buffer.size()) - 1;
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

    // scaleFrame(frame, sx, sy, sz [,subset])  -- frame=0 means all; subset=0 means all
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

    int onGetStrideMeshDebugLua(lua_State *lua)
    {
        MESH_DEBUG_LUA *meshDebug  = getMeshDebugFromRawTable(lua, 1, 1);
        const int       top        = lua_gettop(lua);
        const int       indexFrame = top > 1 ? luaL_checkinteger(lua, 2) - 1 : 0;
        if (indexFrame >= 0 && indexFrame < static_cast<int>(meshDebug->mesh.buffer.size()))
        {
            util::BUFFER_MESH_DEBUG *bufferCurrent = meshDebug->mesh.buffer[indexFrame];
            lua_pushinteger(lua, bufferCurrent->headerFrame.stride);
            return 1;
        }
        return lua_error_debug(lua, "Index frame invalid [%d/%d]", top > 1 ? indexFrame + 1 : indexFrame, meshDebug->mesh.buffer.size());
    }

    int onEnableNormalsMeshDebugLua(lua_State *lua)
    {
        MESH_DEBUG_LUA *meshDebug               = getMeshDebugFromRawTable(lua, 1, 1);
        const int       enable                  = lua_toboolean(lua, 2);
        meshDebug->mesh.headerMesh.hasNorText[0] = enable ? HAS_NOR_IN_FILE : HAS_NOR_NO;
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
                meshDebug->mesh.headerMesh.hasNorText[1] = HAS_TEX_FIRST_FRAME;
            else
                meshDebug->mesh.headerMesh.hasNorText[1] = HAS_TEX_EACH_FRAME;
        }
        else
        {
            meshDebug->mesh.headerMesh.hasNorText[1] = HAS_TEX_NO;
        }
        return 0;
    }

    void fillEffect(const EFFECT_SHADER* fx,const char* textureStage2,util::INFO_SHADER_DATA** dataInfoShader)
    {
        if(fx->getCurrentShader())
        {
            const unsigned int sTexStage2    = textureStage2 ? static_cast<unsigned int>(strlen(textureStage2)): 0;
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
                strncpy(dataInfo->fileNameTextureStage2,textureStage2,sTexStage2 + 1);
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
                FX &fx                      = anim->getFx();
                const char* textureStage2   = fx.textureOverrideStage2 ? fx.textureOverrideStage2->getFileNameTexture() : nullptr;

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
                    meshDebug->mesh.lsBlendOperation[i] = fx.blendOperation;

                    if(fx.fxPS->getCurrentShader())
                    {
                        infoHead->effectShader = new util::INFO_FX();
                        infoHead->effectShader->blendOperation = fx.blendOperation;
                        fillEffect(fx.fxPS,textureStage2,&infoHead->effectShader->dataPS);
                    }
                    if(fx.fxVS->getCurrentShader())
                    {
                        if(infoHead->effectShader == nullptr)
                        {
                            infoHead->effectShader = new util::INFO_FX();
                            infoHead->effectShader->blendOperation = fx.blendOperation;
                        }
                        fillEffect(fx.fxVS,textureStage2,&infoHead->effectShader->dataVS);
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
                                          {"getAngle", onGetAngleMeshDebugLua},
                                          {"setAngle", onSetAngleMeshDebugLua},
                                          {"getPosition", onGetPositionMeshDebugLua},
                                          {"setPosition", onSetPositionMeshDebugLua},
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
                                          {"addFrame", onAddFrameDebugLua},
                                          {"removeFrame", onRemoveFrameDebugLua},
                                          {"addSubSet", onAddSubsetDebugLua},
                                          {"removeSubset", onRemoveSubsetDebugLua},
                                          {"copyFrameFrom", onCopyFrameFromDebugLua},
                                          {"copySubsetFrom", onCopySubsetFromDebugLua},
                                          {"addAnim", onAddAnimationDebugLua},
                                          {"removeAnim", onRemoveAnimationDebugLua},
                                          {"centralize", onCentralizeMeshDebugLua},
                                          {"rotateFrame", onRotateFrameDebugLua},
                                          {"scaleFrame", onScaleFrameDebugLua},
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
