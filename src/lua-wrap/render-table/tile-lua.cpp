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

#include <lua-wrap/render-table/tile-lua.h>
#include <lua-wrap/common-methods-lua.h>
#include <lua-wrap/user-data-lua.h>
#include <lua-wrap/check-user-type-lua.h>
#include <render/tile.h>
#include <core_mbm/util-interface.h>
#include <core_mbm/header-mesh.h>
#include <platform/mismatch-platform.h>
#include <utility>//pair


namespace mbm
{

    extern void printStack(lua_State *lua, const char *fileName, const unsigned int numLine);
    extern RENDERIZABLE *getRenderizableFromRawTable(lua_State *lua, const int rawi, const int indexTable);
    extern void doOffsetIfText(RENDERIZABLE *ptr,const float w,const float h);
    extern void undoOffsetIfText(RENDERIZABLE *ptr,const float w,const float h);
    extern int lua_error_debug(lua_State *lua, const char *format, ...);
    extern void lua_print_line(lua_State *lua, TYPE_LOG type_log, const char *format, ...);

    TILE *getTileFromRawTable(lua_State *lua, const int rawi, const int indexTable)
    {
        auto **ud = static_cast<TILE **>(lua_check_userType(lua, rawi, indexTable, L_USER_TYPE_TILE));
        return *ud;
    }

    TILE_OBJ *getObjTileFromRawTable(lua_State *lua, const int rawi, const int indexTable)
    {
        auto **ud = static_cast<TILE_OBJ **>(lua_check_userType(lua, rawi, indexTable, L_USER_TYPE_TILE_OBJ));
        return *ud;
    }


    bool TILE_FILTER::belongID(const uint16_t _ID)const
    {
        if(ID.size() == 0)
            return true;
        return (ID == std::to_string( _ID));
    }

    bool TILE_FILTER::belong(const util::BTILE_PROPERTY* property)const
    {
        uint16_t _ID = std::numeric_limits<uint16_t>::max();
        const auto p = property->owner.find('-');
        if (p != std::string::npos)
        {
            _ID = std::atoi(property->owner.substr(p+1).c_str());
        }
        if(	belongID(_ID) &&
            belong(property->name,name) &&
            belong(property->owner,owner) &&
            belong(property->type,type) )
            return true;
        return false;
    }

    bool TILE_FILTER::belong(const util::BTILE_OBJ * bObj)const
    {
        if(	belong(bObj->name,name) &&
            belong(bObj->type,type) )
            return true;
        return false;
    }

    bool TILE_FILTER::belong(const std::string & item,const std::string & to_find)const
    {
        if(to_find.size() == 0 )
            return true;
        if(to_find.size() > item.size())
            return false;
        if(item.find(to_find) != std::string::npos )
            return true;
        return false;
    }

    bool TILE_FILTER::belong(const util::BTILE_OBJ_TYPE item,const std::string & to_find)const
    {
        if(to_find.size() == 0 )
            return true;
        std::string nameItemAsString;
        switch (item)
        {
            case util::BTILE_OBJ_TYPE_RECT:     nameItemAsString = "rectangle"; break;
            case util::BTILE_OBJ_TYPE_CIRCLE:   nameItemAsString = "circle";    break;
            case util::BTILE_OBJ_TYPE_POINT:    nameItemAsString = "point";     break;
            case util::BTILE_OBJ_TYPE_TRIANGLE: nameItemAsString = "triangle";  break;
            case util::BTILE_OBJ_TYPE_POLYLINE: nameItemAsString = "line";      break;
            default : nameItemAsString = "unknown"; break;
        }

        if(nameItemAsString.find(to_find) != std::string::npos )
            return true;

        return false;
    }

    bool TILE_FILTER::belong(const util::BTILE_PROPERTY_TYPE item,const std::string & to_find)const
    {
        if(to_find.size() == 0 )
            return true;
        std::string nameItemAsString;
        switch (item)
        {
            case util::BTILE_PROPERTY_TYPE_BOOL:   nameItemAsString = "boolean";  break;
            case util::BTILE_PROPERTY_TYPE_COLOR:  nameItemAsString = "color";    break;
            case util::BTILE_PROPERTY_TYPE_FLOAT:  nameItemAsString = "float";    break;
            case util::BTILE_PROPERTY_TYPE_FILE:   nameItemAsString = "file";     break;
            case util::BTILE_PROPERTY_TYPE_INT:	   nameItemAsString = "int";      break;
            case util::BTILE_PROPERTY_TYPE_STRING: nameItemAsString = "string";   break;
            default : nameItemAsString = "unknown"; break;
        }

        if(nameItemAsString.find(to_find) != std::string::npos )
            return true;
        
        return false;
    }

    int onDestroyTileLua(lua_State *lua)
    {
        TILE *              tile = getTileFromRawTable(lua, 1, 1);
        auto *userData = static_cast<USER_DATA_RENDER_LUA *>(tile->userData);
        if (userData)
        {
            userData->unrefAllTableLua(lua);
            delete userData;
        }
        tile->userData = nullptr;
#if DEBUG_FREE_LUA
        static int  num = 1;
        const char *fileName = tile->getFileName();
        PRINT_IF_DEBUG( "free tile [%s] [%d]\n", fileName ? fileName : "NULL", num++);
#endif
        DEVICE *             device = DEVICE::getInstance();
        auto *userScene = static_cast<USER_DATA_SCENE_LUA *>(device->scene->userData);
        userScene->remove(tile);
        delete tile;
        return 0;
    }

    int onLoadTileLua(lua_State *lua)
    {
        TILE *    tile = getTileFromRawTable(lua, 1, 1);
        const char *fileName = luaL_checkstring(lua, 2);
        if (tile->getFileName() && strcmp(tile->getFileName(), fileName) == 0)
        {
            lua_pushboolean(lua, 1);
            return 1;
        }
        else
        {
            tile->release();
        }
        if (tile->load(fileName))
            lua_pushboolean(lua, 1);
        else
            lua_pushboolean(lua, 0);
        return 1;
    }

    int onSetBrickIDTileObj(lua_State *lua)
    {
        TILE_OBJ *          tileObj = getObjTileFromRawTable(lua, 1, 1);
        const uint32_t brickID      = luaL_checkinteger(lua,2);
        tileObj->setBrickID(brickID);
        return 0;
    }

    static void fillListFilterTile(lua_State *lua,const int index, std::string & to_find,const char* filter)
    {
        lua_getfield(lua,index,filter);
        const int lType = lua_type(lua,-1);
        if (lType == LUA_TSTRING || lType == LUA_TNUMBER)
        {
            const char* name = lua_tostring(lua,-1);
            to_find = (name ? name : "");
        }
        lua_pop(lua,1);
    }
    

    int onGetPropertyLuaTile(lua_State *lua)
    {
        TILE *    tile                     = getTileFromRawTable(lua, 1, 1);
        const util::BTILE_INFO * bTileInfo = tile->getTileInfo();
        const int top                      = lua_gettop(lua);
        unsigned int iMax                  = 0xffffffff;
        mbm::TILE_FILTER filter;
        if (top >= 2 && lua_istable(lua,2))
        {
            fillListFilterTile(lua,2,filter.name,"name");
            fillListFilterTile(lua,2,filter.owner,"owner");
            fillListFilterTile(lua,2,filter.type,"type");
            fillListFilterTile(lua,2,filter.ID,"ID");
        }
        lua_settop(lua,0);
        lua_newtable(lua);
        if(bTileInfo)
        {
            int index = 1;
            unsigned int iTotal = 0;
            for (unsigned int i = 0; i < bTileInfo->lsProperty.size() && iTotal < iMax; ++i )
            {
                const auto property = bTileInfo->lsProperty[i];
                if (filter.belong(property) == false)
                    continue;
                ++iTotal;
                lua_newtable(lua);
                lua_pushstring(lua,property->name.c_str());
                lua_setfield(lua, -2, "name");
                lua_pushstring(lua,property->owner.c_str());
                lua_setfield(lua, -2, "owner");
                switch(property->type)
                {
                    case util::BTILE_PROPERTY_TYPE_BOOL:
                    {
                        lua_pushstring(lua,"boolean");
                        lua_setfield(lua, -2, "type");
                        lua_pushboolean(lua,strcmp(property->value.c_str(),"true") == 0 ? 1 : 0);
                    }
                    break;
                    case util::BTILE_PROPERTY_TYPE_COLOR:
                    {
                        lua_pushstring(lua,"color");
                        lua_setfield(lua, -2, "type");
                        COLOR color = COLOR::getColorFromHexString(property->value.c_str());
                        lua_newtable(lua);
                        lua_pushnumber(lua,color.r);
                        lua_setfield(lua, -2, "r");
                        lua_pushnumber(lua,color.g);
                        lua_setfield(lua, -2, "g");
                        lua_pushnumber(lua,color.b);
                        lua_setfield(lua, -2, "b");
                        lua_pushnumber(lua,color.a);
                        lua_setfield(lua, -2, "a");
                    }
                    break;
                    case util::BTILE_PROPERTY_TYPE_FLOAT:
                    {
                        lua_pushstring(lua,"float");
                        lua_setfield(lua, -2, "type");
                        lua_pushnumber(lua,static_cast<float>(atof(property->value.c_str())));
                    }
                    break;
                    case util::BTILE_PROPERTY_TYPE_FILE:
                    case util::BTILE_PROPERTY_TYPE_STRING:
                    {
                        if(property->type == util::BTILE_PROPERTY_TYPE_FILE)
                            lua_pushstring(lua,"file");
                        else
                            lua_pushstring(lua,"string");
                        lua_setfield(lua, -2, "type");
                        lua_pushstring(lua,property->value.c_str());
                    }
                    break;
                    case util::BTILE_PROPERTY_TYPE_INT:
                    {
                        lua_pushstring(lua,"int");
                        lua_setfield(lua, -2, "type");
                        lua_pushinteger(lua,std::atoi(property->value.c_str()));
                    }
                    break;
                    default:
                    {
                        lua_pushstring(lua,property->value.c_str());
                    }
                    break;
                }
                lua_setfield(lua, -2, "value");
                lua_rawseti(lua, -2, index++);
            }
        }
        return 1;
    }

    int push_object(lua_State *lua,TILE * tile,const mbm::TILE_FILTER & filter)
    {
        const util::BTILE_INFO * bTileInfo = tile->getTileInfo();
        lua_newtable(lua);
        if(bTileInfo)
        {
            int index = 1;
            for (unsigned int i=0; i< bTileInfo->lsObj.size(); ++i)
            {
                const auto obj = bTileInfo->lsObj[i];
                if (filter.belong(obj) == false)
                    continue;
                lua_newtable(lua);
                lua_pushstring(lua,obj->name.c_str());
                lua_setfield(lua, -2, "name");
                switch(obj->type)
                {
                    case util::BTILE_OBJ_TYPE_RECT:
                    {
                        const unsigned int s = obj->lsPoints.size();
                        const float px       = (s > 0 ? obj->lsPoints[0]->x * tile->scale.x: 0.0f) + tile->position.x;
                        const float py       = (s > 0 ? obj->lsPoints[0]->y * tile->scale.y: 0.0f) + tile->position.y;
                        const float width    =  s > 1 ? obj->lsPoints[1]->x * tile->scale.x: 0.0f;
                        const float height   =  s > 1 ? obj->lsPoints[1]->y * tile->scale.y: 0.0f;
                        lua_pushstring(lua,"rectangle");
                        lua_setfield(lua, -2, "type");
                        lua_pushnumber(lua,width);
                        lua_setfield(lua, -2, "width");
                        lua_pushnumber(lua,height);
                        lua_setfield(lua, -2, "height");
                        lua_pushnumber(lua,px);
                        lua_setfield(lua, -2, "x");
                        lua_pushnumber(lua,py);
                        lua_setfield(lua, -2, "y");
                    }
                    break;
                    case util::BTILE_OBJ_TYPE_CIRCLE:
                    {
                        const unsigned int s = obj->lsPoints.size();
                        lua_pushstring(lua,"circle");
                        lua_setfield(lua, -2, "type");
                        const float px       = (s > 0 ? obj->lsPoints[0]->x * tile->scale.x: 0.0f)  + tile->position.x;
                        const float py       = (s > 0 ? obj->lsPoints[0]->y * tile->scale.y: 0.0f)  + tile->position.y;
                        lua_pushnumber(lua,px);
                        lua_setfield(lua, -2, "x");
                        lua_pushnumber(lua,py);
                        lua_setfield(lua, -2, "y");
                        const float width    = s > 1 ? obj->lsPoints[1]->x * tile->scale.x: 0.0f;
                        lua_pushnumber(lua,width);
                        lua_setfield(lua, -2, "ray");
                    }
                    break;
                    case util::BTILE_OBJ_TYPE_POLYLINE:
                    {
                        const unsigned int s = obj->lsPoints.size();
                        lua_pushstring(lua,"line");
                        lua_setfield(lua, -2, "type");
                        lua_newtable(lua);
                        for (unsigned int j = 0; j < s; ++j)
                        {
                            const VEC2* point = obj->lsPoints[j];
                            lua_newtable(lua);
                            lua_pushnumber(lua,point->x * tile->scale.x);
                            lua_setfield(lua, -2, "x");
                            lua_pushnumber(lua,point->y * tile->scale.y);
                            lua_setfield(lua, -2, "y");
                            lua_rawseti(lua, -2, j+1);
                        }
                        lua_setfield(lua, -2, "points");
                    }
                    break;
                    case util::BTILE_OBJ_TYPE_POINT:
                    {
                        const unsigned int s = obj->lsPoints.size();
                        const float px       = (s > 0 ? obj->lsPoints[0]->x * tile->scale.x: 0.0f) + tile->position.x;
                        const float py       = (s > 0 ? obj->lsPoints[0]->y * tile->scale.y: 0.0f) + tile->position.y;
                        lua_pushstring(lua,"point");
                        lua_setfield(lua, -2, "type");
                        lua_pushnumber(lua,px);
                        lua_setfield(lua, -2, "x");
                        lua_pushnumber(lua,py);
                        lua_setfield(lua, -2, "y");
                    }
                    break;
                    case util::BTILE_OBJ_TYPE_TRIANGLE:
                    {
                        const unsigned int s = obj->lsPoints.size();
                        lua_pushstring(lua,"triangle");
                        lua_setfield(lua, -2, "type");
                        for (int j = 0; j < 3; j++)
                        {
                            const float px       = (s > 0 ? obj->lsPoints[j]->x * tile->scale.x: 0.0f) + tile->position.x;
                            const float py       = (s > 0 ? obj->lsPoints[j]->y * tile->scale.y: 0.0f) + tile->position.y;
                            lua_newtable(lua);
                            lua_pushnumber(lua,px);
                            lua_setfield(lua, -2, "x");
                            lua_pushnumber(lua,py);
                            lua_setfield(lua, -2, "y");
                            char p[3] = { 'p','1',0};
                            p[1] += static_cast<char>(j);  
                            lua_setfield(lua,-2,p);
                        }
                    }
                    break;
                    default:
                    {
                        lua_error_debug(lua, "Object type [%d] not handled ",obj->type);
                        return 0;
                    }
                    break;
                }
                lua_rawseti(lua, -2, index++);
            }
        }
        return 1;
    }

    int onGetObjectLuaTile(lua_State *lua)
    {
        TILE *    tile                     = getTileFromRawTable(lua, 1, 1);
        const int top                      = lua_gettop(lua);
        mbm::TILE_FILTER filter;
        if (top >= 2 && lua_istable(lua,2))
        {
            fillListFilterTile(lua,2,filter.name,"name");
            fillListFilterTile(lua,2,filter.type,"type");
            fillListFilterTile(lua,2,filter.ID,"ID");
        }
        lua_settop(lua,0);
        return push_object(lua,tile,filter);
    }

    static bool pushTileObj(lua_State *lua, TILE_OBJ* tile_obj)
    {
        if (tile_obj)
        {
            luaL_Reg  regMethodsTile[] = {
                {"setBrickID", onSetBrickIDTileObj},
                { nullptr, nullptr } 
            };

            SELF_ADD_COMMON_METHODS selfMethods(regMethodsTile);
            const luaL_Reg *             regMethods = selfMethods.get();
            
            lua_createtable(lua, 0, selfMethods.getSize());
            luaL_setfuncs(lua, regMethods, 0);
            luaL_getmetatable(lua, "_mbmObjTile");
            lua_setmetatable(lua, -2);
            
            auto **      udata = static_cast<TILE_OBJ **>(lua_newuserdata(lua, sizeof(TILE_OBJ *)));
            if(tile_obj->userData == nullptr)
            {
                auto tableLuaMbm = new USER_DATA_RENDER_LUA();
                tile_obj->userData = tableLuaMbm;
            }
            *udata = tile_obj;

            // trick to ensure that we will receive the expected metatable type expected metatable type. 
            const char* __userdata_name = getUserTypeAsString(L_USER_TYPE_TILE_OBJ);
            luaL_getmetatable(lua, __userdata_name);
            lua_setmetatable(lua, -2);
            // end trick 
            lua_rawseti(lua, -2, 1);
        }
        else
        {
            lua_pushnil(lua);
        }
        return 1;
    }

    int onBuildObjectsForRenderLuaTile(lua_State *lua)
    {
        TILE *    tile                = getTileFromRawTable(lua, 1, 1);
        const uint32_t tileID         = luaL_checkinteger(lua,2);
        const int indexLayer          = luaL_checkinteger(lua,3);
        lua_settop(lua,0);
        lua_newtable(lua);
        const util::BTILE_INFO * pInfo = tile->getTileInfo();
        if(pInfo == nullptr)
        {
            lua_print_line(lua,TYPE_LOG_ERROR,"Tile map is not Loaded ");
            return 0;
        }
        if(indexLayer < 1 || indexLayer > static_cast<int>(tile->getTotalLayer()))
        {
            lua_print_line(lua,TYPE_LOG_ERROR,"indexLayer [%d] out of range [1-%d]",indexLayer,tile->getTotalLayer());
            return 0;
        }
        if(tileID < 1 || tileID > (pInfo->map.count_width_tile * pInfo->map.count_height_tile))
        {
            lua_print_line(lua,TYPE_LOG_ERROR,"tile ID [%d] out of range [1-%d]",tileID,(pInfo->map.count_width_tile * pInfo->map.count_height_tile));
            return 0;
        }
        TILE_OBJ * tileObj = tile->buildTileRenderizable(tileID-1,indexLayer-1);
        return pushTileObj(lua,tileObj);
    }

    int onSetLayerVisibleLuaTile(lua_State *lua)
    {
        TILE *    tile = getTileFromRawTable(lua, 1, 1);
        const unsigned int index = luaL_checkinteger(lua,2);
        const bool visible = lua_toboolean(lua,3) > 0 ? true : false;
        if(index < 1 || index > tile->getTotalLayer())
            lua_print_line(lua,TYPE_LOG_ERROR,"index [%d] out of range [1-%d]",index,tile->getTotalLayer());
        else
            tile->setLayerVisible(index - 1,visible);
        return 0;
    }

    int onGetTotalLayerLuaTile(lua_State *lua)
    {
        TILE *    tile = getTileFromRawTable(lua, 1, 1);
        const unsigned int iTotalLayer = tile->getTotalLayer();
        lua_pushinteger(lua,iTotalLayer);
        return 1;
    }
        
    int onDisableTileLuaTile(lua_State *lua)
    {
        TILE *    tile 		 = getTileFromRawTable(lua, 1, 1);
        const int top 		 = lua_gettop(lua);
        unsigned int px 	 = luaL_checkinteger(lua,2);
        unsigned int py 	 = luaL_checkinteger(lua,3);
        const int indexLayer = top > 3 ? luaL_checkinteger(lua,4) -1 : -1;
        const uint32_t index = tile->disableRenderTile(px,py,indexLayer);
        lua_pushinteger(lua,index == std::numeric_limits<uint32_t>::max() ? 0 : index + 1);
        return 1;
    }

    int onEnableTileLuaTile(lua_State *lua)
    {
        TILE *    tile 		 = getTileFromRawTable(lua, 1, 1);
        const int top 		 = lua_gettop(lua);
        unsigned int px 	 = luaL_checkinteger(lua,2);
        unsigned int py 	 = luaL_checkinteger(lua,3);
        const int indexLayer = top > 3 ? luaL_checkinteger(lua,4) -1 : -1;
        const uint32_t index = tile->enableRenderTile(px,py,indexLayer);
        lua_pushinteger(lua,index == std::numeric_limits<uint32_t>::max() ? 0 : index + 1);
        return 1;
    }

    int onGetPositionsFromBrickIDLua(lua_State *lua)
    {
        const int top               = lua_gettop(lua);
        TILE *    tile              = getTileFromRawTable(lua, 1, 1);
        const unsigned int BrickID  = luaL_checkinteger(lua,2) - 1;
        const int indexLayer        = top > 2 ? luaL_checkinteger(lua,3)-1 : -1;
        auto positionsIdOut         = tile->getPositionsFromBrickID(BrickID, indexLayer);
        lua_newtable(lua);
        for (unsigned int i=0; i< positionsIdOut.size(); ++i)
        {
            lua_newtable(lua);
            const auto & p = positionsIdOut[i];
            lua_pushinteger(lua,std::get<0>(p)+1);
            lua_setfield(lua, -2, "layer");
            lua_pushnumber(lua,std::get<1>(p));
            lua_setfield(lua, -2, "x");
            lua_pushnumber(lua,std::get<2>(p));
            lua_setfield(lua, -2, "y");
            lua_rawseti(lua, -2, i+1);
        }
        return 1;
    }

    int onGetPositionFromTileID(lua_State *lua)
    {
        TILE *    tile              = getTileFromRawTable(lua, 1, 1);
        float x = 0;
        float y = 0;
        const unsigned int tileID   = luaL_checkinteger(lua,2) - 1;
        if(tile->getPositionFromTileID(tileID,x,y))
        {
            lua_pushnumber(lua,x);
            lua_pushnumber(lua,y);
        }
        else
        {
            lua_pushnil(lua);
            lua_pushnil(lua);
        }
        return 2;
    }
    
    int onGetNearPositionFromTileID(lua_State *lua)
    {
        const int top               = lua_gettop(lua);
        TILE *    tile              = getTileFromRawTable(lua, 1, 1);
        float x                     = luaL_checknumber(lua,2);
        float y                     = luaL_checknumber(lua,3);
        const uint32_t indexLayer   = top > 3 ? luaL_checkinteger(lua,4) - 1 : 0;
        tile->getNearPosition(x, y, indexLayer);
        lua_settop(lua,0);
        lua_pushnumber(lua,x);
        lua_pushnumber(lua,y);
        return 2;
    }

    int onGetTileSizeLuaTile(lua_State *lua)
    {
        TILE *    tile  = getTileFromRawTable(lua, 1, 1);
        float width     = 0.0f;
        float height    = 0.0f;
        tile->getTileSize(width,height);
        lua_pushnumber(lua,width);
        lua_pushnumber(lua,height);
        return 2;
    }

    int onGetMapSizeSizeLuaTile(lua_State *lua)
    {
        TILE *    tile  = getTileFromRawTable(lua, 1, 1);
        float width     = 0.0f;
        float height    = 0.0f;
        tile->getMapSize(width,height);
        lua_pushnumber(lua,width);
        lua_pushnumber(lua,height);
        return 2;
    }

    int onGetTypeMapLuaTile(lua_State *lua)
    {
        TILE *    tile = getTileFromRawTable(lua, 1, 1);
        const util::BTILE_INFO * pInfo = tile->getTileInfo();
        if(pInfo == nullptr)
        {
            lua_pushstring(lua,"unknown");
            return 1;
        }
        switch (pInfo->map.typeMap)
        {
            case util::BTILE_TYPE_ORIENTATION_HEXAGONAL  : lua_pushstring(lua,"hexagonal"); break;
            case util::BTILE_TYPE_ORIENTATION_ISOMETRIC  : lua_pushstring(lua,"isometric"); break;
            case util::BTILE_TYPE_ORIENTATION_ORTHOGONAL : lua_pushstring(lua,"orthogonal"); break;
            case util::BTILE_TYPE_ORIENTATION_STAGGERED  : lua_pushstring(lua,"isometric staggered"); break;
            default:lua_pushstring(lua,"unknown");  break;
        }
        return 1;
    }

    int onSetTileIDTile(lua_State *lua)
    {
        TILE * tile                 = getTileFromRawTable(lua, 1, 1);
        const int x                 = luaL_checkinteger(lua,2);
        const int y                 = luaL_checkinteger(lua,3);
        const uint32_t brickID      = luaL_checkinteger(lua,4);
        const uint32_t indexLayer   = luaL_checkinteger(lua,5) - 1;
        if (tile->setTileID(x, y, brickID, indexLayer))
            lua_pushboolean(lua,1);
        else
            lua_pushboolean(lua,0);
        return 1;
    }

    
    int onGetLayerZTile(lua_State *lua)
    {
        TILE * tile                 = getTileFromRawTable(lua, 1, 1);
        const uint32_t indexLayer   = luaL_checkinteger(lua,2);
        if(indexLayer < 1 || indexLayer > tile->getTotalLayer())
        {
            lua_print_line(lua,TYPE_LOG_ERROR,"layer number [%d] out of range [1-%d]",indexLayer,tile->getTotalLayer());
            lua_pushnumber(lua,0);
        }
        else
        {
            lua_pushnumber(lua,tile->getZLayer(indexLayer-1));
        }
        return 1;
    }

    int onSetLayerZTile(lua_State *lua)
    {
        TILE * tile                 = getTileFromRawTable(lua, 1, 1);
        const uint32_t indexLayer   = luaL_checkinteger(lua,2);
        const float  z              = luaL_checknumber(lua,3);
        if(indexLayer < 1 || indexLayer > tile->getTotalLayer())
        {
            lua_print_line(lua,TYPE_LOG_ERROR,"layer number [%d] out of range [1-%d]",indexLayer,tile->getTotalLayer());
        }
        else
        {
            tile->setZLayer(indexLayer-1,z);
        }
        return 0;
    }


    int onGetTileIDTile(lua_State *lua)
    {
        TILE * tile                 = getTileFromRawTable(lua, 1, 1);
        const int x                 = luaL_checkinteger(lua,2);
        const int y                 = luaL_checkinteger(lua,3);
        const uint32_t indexLayer   = luaL_checkinteger(lua,4);
        uint16_t brick_ID_found     = std::numeric_limits<uint16_t>::max() - 1;
        if(indexLayer < 1 || indexLayer > tile->getTotalLayer())
        {
            lua_print_line(lua,TYPE_LOG_ERROR,"layer number [%d] out of range [1-%d]",indexLayer,tile->getTotalLayer());
            lua_pushinteger(lua,0);
            lua_pushinteger(lua,0);
        }
        else
        {
            const uint16_t tile_ID_found      = tile->getTileID(static_cast<float>(x), static_cast<float>(y), indexLayer-1,&brick_ID_found);
            if (tile_ID_found < std::numeric_limits<uint16_t>::max()-1)
            {
                lua_pushinteger(lua,tile_ID_found+1);
                if(brick_ID_found < std::numeric_limits<uint16_t>::max() - 1 )
                    lua_pushinteger(lua,brick_ID_found+1);
                else
                    lua_pushinteger(lua,0);
            }
            else
            {
                lua_pushinteger(lua,0);
                lua_pushinteger(lua,0);
            }
        }
        return 2;
    }
    
    int onGetPhysicsLuaTile(lua_State *lua)
    {
        mbm::TILE_FILTER filter;
        TILE * tile                    = getTileFromRawTable(lua, 1, 1);
        const uint32_t brickID         = luaL_checkinteger(lua,2);
        filter.name                    = "physic-";
        filter.name                   += std::to_string(brickID);
        lua_settop(lua,0);
        return push_object(lua,tile,filter);
    }

    int onDestroyObjTileLua(lua_State *lua)
    {
        TILE_OBJ *              tileObj = getObjTileFromRawTable(lua, 1, 1);
        auto *userData = static_cast<USER_DATA_RENDER_LUA *>(tileObj->userData);
        if (userData)
        {
            userData->unrefAllTableLua(lua);
            delete userData;
        }
        tileObj->userData = nullptr;
#if DEBUG_FREE_LUA
        static int  num = 1;
        const char *fileName = tileObj->getFileName();
        PRINT_IF_DEBUG( "free tile_obj [%s] [%d]\n", fileName ? fileName : "NULL", num++);
#endif
        DEVICE * device = DEVICE::getInstance();
        auto *userScene = static_cast<USER_DATA_SCENE_LUA *>(device->scene->userData);
        userScene->remove(tileObj);
        return 0;
    }

    int onNewTileLua(lua_State *lua)
    {
        const int top = lua_gettop(lua);
        luaL_Reg  regTileMethods[] = {  { "load", onLoadTileLua },
                                        { "getProperties", onGetPropertyLuaTile},
                                        { "getObjects", onGetObjectLuaTile},
                                        { "createTileObject", onBuildObjectsForRenderLuaTile},
                                        { "setLayerVisible", onSetLayerVisibleLuaTile},
                                        { "getTypeMap", onGetTypeMapLuaTile},
                                        { "getTotalLayer", onGetTotalLayerLuaTile},
                                        { "disableTile", onDisableTileLuaTile},
                                        { "enableTile", onEnableTileLuaTile},
                                        { "getPositionsFromBrickID", onGetPositionsFromBrickIDLua},
                                        { "getPositionFromTileID", onGetPositionFromTileID},
                                        { "getNearPosition", onGetNearPositionFromTileID},
                                        { "getTileSize", onGetTileSizeLuaTile},
                                        { "getMapSize", onGetMapSizeSizeLuaTile},
                                        { "getPhysicsBrick", onGetPhysicsLuaTile},
                                        { "setTileID", onSetTileIDTile},
                                        { "getTileID", onGetTileIDTile},
                                        { "getLayerZ", onGetLayerZTile},
                                        { "setLayerZ", onSetLayerZTile},
                                        { nullptr, nullptr } };

        SELF_ADD_COMMON_METHODS selfMethods(regTileMethods);
        const luaL_Reg *             regMethods = selfMethods.get();

        VEC3 position(0, 0, 0);
        bool is2dw = true;
        bool is2ds = true;
        bool is3d = false;
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
        luaL_getmetatable(lua, "_mbmTile");
        lua_setmetatable(lua, -2);
        DEVICE *         device = DEVICE::getInstance();
        auto **           udata = static_cast<TILE **>(lua_newuserdata(lua, sizeof(TILE *)));
        auto               tile = new TILE(device->scene, is3d, is2ds);
        auto tableLuaMbm        = new USER_DATA_RENDER_LUA();
        tile->userData          = tableLuaMbm;
        *udata = tile;
        if (position.x != 0.0f) //-V550
            tile->position.x = position.x;
        if (position.y != 0.0f) //-V550
            tile->position.y = position.y;
        if (position.z != 0.0f) //-V550
            tile->position.z = position.z;

        /* trick to ensure that we will receive the expected metatable type expected metatable type. */
        const char* __userdata_name = getUserTypeAsString(L_USER_TYPE_TILE);
        luaL_getmetatable(lua, __userdata_name);
        lua_setmetatable(lua, -2);
        /* end trick */

        lua_rawseti(lua, -2, 1);
        return 1;
    }

    static void registerClassObjTile(lua_State *lua)
    {
        luaL_Reg regObjTileMethods[] = { 
            { "__newindex", onNewIndexRenderizableLua },
            { "__index", onIndexRenderizableLua },
            { "__gc", onDestroyObjTileLua },
            {"__close", onDestroyRenderizable},
            { nullptr, nullptr } };
        luaL_newmetatable(lua, "_mbmObjTile");
        luaL_setfuncs(lua, regObjTileMethods, 0);
        lua_settop(lua, 0);
    }

    void registerClassTile(lua_State *lua)
    {
        luaL_Reg regTileMethods[] = { { "new", onNewTileLua },
        { "__newindex", onNewIndexRenderizableLua },
        { "__index", onIndexRenderizableLua },
        { "__gc", onDestroyTileLua },
        {"__close", onDestroyRenderizable},
        { nullptr, nullptr } };
        luaL_newmetatable(lua, "_mbmTile");
        luaL_setfuncs(lua, regTileMethods, 0);
        lua_setglobal(lua, "tile");
        lua_settop(lua, 0);
        registerClassObjTile(lua);
    }
};
