/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT);                                                                                                     |
| Copyright (C); 2020      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                      |
|                                                                                                                        |
| Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated           |
| documentation files (the "Software");, to deal in the Software without restriction, including without limitation       |
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


#include "tiled-lua.h"
#include "tile_editor.hpp"
#include "../plugin-helper/plugin-helper.h"

extern "C" 
{
    #include <lua.h>
    #include <lauxlib.h>
    #include <lualib.h>
}

#include <string>
#include <core_mbm/util-interface.h>
#include <core_mbm/texture-manager.h>
#include <core_mbm/dynamic-var.h>

static int PLUGIN_IDENTIFIER = 1; //this value is auto set by this module. It is set in the metatable to make sure that we can convert the userdata to ** mbm::TILE_EDITOR


mbm::TILE_EDITOR *getTileEditorFromRawTable(lua_State *lua, const int rawi, const int indexTable);

mbm::TILE_EDITOR *getTileEditorFromRawTable(lua_State *lua, const int rawi, const int indexTable)
{
    const int typeObj = lua_type(lua, indexTable);
    if (typeObj != LUA_TTABLE)
    {
        if(typeObj == LUA_TNONE)
            plugin_helper::lua_error_debug(lua, "expected: [plugin]. got [nil]");
        else
        {
            char message[255] = "";
            snprintf(message,sizeof(message),"expected: [plugin]. got [%s]",lua_typename(lua, typeObj));
            plugin_helper::lua_error_debug(lua, message);
        }
        return nullptr;
    }
    lua_rawgeti(lua, indexTable, rawi);
    void *p = lua_touserdata(lua, -1);
    if (p != nullptr) 
    {  /* value is a userdata? */
        if (lua_getmetatable(lua, -1))
        {  /* does it have a metatable? */
            lua_rawgeti(lua,-1, 1);
            const int L_USER_TYPE_PLUGIN  = lua_tointeger(lua,-1);
            lua_pop(lua, 3);
            if(L_USER_TYPE_PLUGIN == PLUGIN_IDENTIFIER)//Is it really a plugin defined by the engine ?
            {
                mbm::TILE_EDITOR **ud = static_cast<mbm::TILE_EDITOR **>(p);
                if(ud && *ud)
                    return *ud;
            }
        }
        else
        {
            lua_pop(lua, 2);
        }
    }
    else
    {
        lua_pop(lua, 1);
    }
    return nullptr;
}

void lua_check_is_table(lua_State *lua, const int index,const char * table_name)
{
    if (lua_type(lua,index) != LUA_TTABLE)
    {
        plugin_helper::lua_error_debug(lua,"Expected table [%s]",table_name);
    }
}

void lua_push_properties(lua_State * lua, const std::map<std::string,std::shared_ptr<mbm::DYNAMIC_VAR>>  & properties)
{
    lua_newtable(lua);
    int index_lua = 1;
    for (const auto & property : properties)
    {
        lua_newtable(lua);
        lua_pushstring(lua,property.first.c_str());
        lua_setfield(lua,-2,"name");
        switch (property.second->type)
        {
            case mbm::DYNAMIC_BOOL:
            {
                lua_pushboolean(lua,property.second->getBool());
                lua_setfield(lua,-2,"value");
                lua_pushstring(lua,"Boolean");
                lua_setfield(lua,-2,"type");
            }
            break;
            case mbm::DYNAMIC_FLOAT:
            {
                lua_pushnumber(lua,property.second->getFloat());
                lua_setfield(lua,-2,"value");
                lua_pushstring(lua,"Number");
                lua_setfield(lua,-2,"type");
            }
            break;
            case mbm::DYNAMIC_CSTRING:
            {
                lua_pushstring(lua,property.second->getString());
                lua_setfield(lua,-2,"value");
                lua_pushstring(lua,"Text");
                lua_setfield(lua,-2,"type");
            }
            break;
            default:
            {
                plugin_helper::lua_error_debug(lua,"Not expected type of property [%d]",property.second->type);
            }
            break;
        }
        lua_rawseti(lua,-2, index_lua++);
    }
}

void lua_set_properties(lua_State * lua, int index,std::map<std::string,std::shared_ptr<mbm::DYNAMIC_VAR>>  & properties)
{
    lua_check_is_table(lua, index, "{name, value}");
    lua_getfield(lua, index, "name");
    const char * name = luaL_checkstring(lua,-1);
    lua_pop(lua, 1);
    lua_getfield(lua, index, "value");
    const int type    = lua_type(lua,-1);
    switch (type)
    {
        case LUA_TNUMBER:
        {
            const float value = luaL_checknumber(lua,-1);
            auto & var = properties[name];
            if(var == nullptr)
            {
                properties[name] = std::make_shared<mbm::DYNAMIC_VAR>(mbm::DYNAMIC_FLOAT,&value);
            }
            else if(var->type != mbm::DYNAMIC_FLOAT)
            {
                var.reset(new mbm::DYNAMIC_VAR(mbm::DYNAMIC_FLOAT,&value));
            }
            else
            {
                var->setFloat(value);
            }
        }
        break;
        case LUA_TBOOLEAN:
        {
            const bool value = lua_toboolean(lua,-1);
            auto & var = properties[name];
            if(var == nullptr)
            {
                properties[name] = std::make_shared<mbm::DYNAMIC_VAR>(mbm::DYNAMIC_BOOL,&value);
            }
            else if(var->type != mbm::DYNAMIC_BOOL)
            {
                var.reset(new mbm::DYNAMIC_VAR(mbm::DYNAMIC_BOOL,&value));
            }
            else
            {
                var->setBool(value);
            }
        }
        break;
        case LUA_TSTRING:
        {
            const char * value = luaL_checkstring(lua,-1);
            auto & var = properties[name];
            if(var == nullptr)
            {
                properties[name] = std::make_shared<mbm::DYNAMIC_VAR>(mbm::DYNAMIC_CSTRING,value);
            }
            else if(var->type != mbm::DYNAMIC_CSTRING)
            {
                var.reset(new mbm::DYNAMIC_VAR(mbm::DYNAMIC_CSTRING,value));
            }
            else
            {
                var->setString(value);
            }
        }
        break;
        default:
        {
            plugin_helper::lua_error_debug(lua,"Value expected [number,string,boolean], got [%s]",lua_typename(lua,type));
        }
        break;
    }
}

void lua_push_rgba(lua_State * lua, const mbm::COLOR & color)
{
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

void lua_push_vec2(lua_State * lua,const mbm::VEC2 & vec)
{
    lua_newtable(lua);
    lua_pushnumber(lua,vec.x);
    lua_setfield(lua, -2, "x");

    lua_pushnumber(lua,vec.y);
    lua_setfield(lua, -2, "y");
}

mbm::VEC2 lua_pop_vec2(lua_State * lua,int index)
{
    mbm::VEC2 vec2;
    lua_check_is_table(lua, index, "{x,y}");
    lua_getfield(lua, index, "x");
    vec2.x = luaL_checknumber(lua,-1);
    lua_pop(lua, 1);

    lua_getfield(lua, index, "y");
    vec2.y = luaL_checknumber(lua,-1);
    lua_pop(lua, 1);

    return vec2;
}

template< typename T >
struct array_deleter
{
  void operator ()( T const * p)
  { 
    delete[] p; 
  }
};

std::shared_ptr<mbm::VEC2> lua_pop_line(lua_State * lua,const int index,uint32_t & size_line)
{
    lua_check_is_table(lua, index, "{x,y,x,y,...}");
    size_line = lua_rawlen(lua,index) / 2;
    auto vec2 = std::shared_ptr<mbm::VEC2>(new mbm::VEC2[size_line],array_deleter<mbm::VEC2>());
    auto vec = vec2.get();
    for(int i = 1, j=0; i <= static_cast<int>(size_line * 2); i +=2, j++)
    {
        lua_rawgeti(lua,index,i);
        const float x  = luaL_checknumber(lua,-1);
        lua_pop(lua,1);

        lua_rawgeti(lua,index,i+1);
        const float y  = luaL_checknumber(lua,-1);
        lua_pop(lua,1);

        vec[j].x = x;
        vec[j].y = y;
    }
    return vec2;
}


mbm::COLOR lua_pop_rgba(lua_State * lua, int index)
{
    mbm::COLOR color;
    lua_check_is_table(lua, index, "{r,g,b,a}");
    lua_getfield(lua, index, "r");
    color.r = luaL_checknumber(lua,-1);
    lua_pop(lua, 1);
    lua_getfield(lua, index, "g");
    color.g = luaL_checknumber(lua,-1);
    lua_pop(lua, 1);
    lua_getfield(lua, index, "b");
    color.b = luaL_checknumber(lua,-1);
    lua_pop(lua, 1);
    lua_getfield(lua, index, "a");
    if (lua_type(lua,-1) == LUA_TNUMBER)
        color.a = lua_tonumber(lua,-1);
    lua_pop(lua, 1);
    return color;
}

int onLoadTiledEditorLua(lua_State *lua)
{
	mbm::TILE_EDITOR *    tileEditor = getTileEditorFromRawTable(lua,1,1);
	const char* fileName               = luaL_checkstring(lua,2);
    if(tileEditor->loadBinary(fileName))
    {
        lua_pushboolean(lua,1);
    }
    else
    {
        lua_pushboolean(lua,0);
    }
	return 1;
}

static int onSavetileEditor(lua_State *lua)
{
	mbm::TILE_EDITOR *    tileEditor   = getTileEditorFromRawTable(lua,1,1);
    const char* fileName               = luaL_checkstring(lua,2);
    if(tileEditor->saveBinary(fileName))
        lua_pushboolean(lua,1);
    else
        lua_pushboolean(lua,0);
    return 1;
}

static int onSelectTilesLua(lua_State *lua)
{
	mbm::TILE_EDITOR *    tileEditor   = getTileEditorFromRawTable(lua,1,1);
    mbm::VEC2 start                    = lua_pop_vec2(lua,2);
    mbm::VEC2 end                      = lua_pop_vec2(lua,3);
    tileEditor->selectTiles(start,end);
    return 0;
}


static int onGetVersion(lua_State *lua)
{
    lua_pushstring(lua,TILE_EDITOR_VERSION);
    return 1;
}


//static int onIsLoadedtileEditor(lua_State *lua)
//{
//    mbm::TILE_EDITOR *    tileEditor   = getTileEditorFromRawTable(lua,1,1);
//    return 0;
//}

static int onGetMapCountWidthTiledEditorLua(lua_State * lua)
{
    mbm::TILE_EDITOR * tileEditor  = getTileEditorFromRawTable(lua,1,1);
    lua_pushinteger(lua,tileEditor->getMapCountWidth());
    return 1;
}
static int onSetMapCountWidthTiledEditorLua(lua_State * lua)
{
    mbm::TILE_EDITOR * tileEditor  = getTileEditorFromRawTable(lua,1,1);
    tileEditor->setMapCountWidth(luaL_checkinteger(lua,2));
    return 0;
}
static int onGetMapCountHeightTiledEditorLua(lua_State * lua)
{
    mbm::TILE_EDITOR * tileEditor  = getTileEditorFromRawTable(lua,1,1);
    lua_pushinteger(lua,tileEditor->getMapCountHeight());
    return 1;
}
static int onSetMapCountHeightTiledEditorLua(lua_State * lua)
{
    mbm::TILE_EDITOR * tileEditor  = getTileEditorFromRawTable(lua,1,1);
    tileEditor->setMapCountHeight(luaL_checkinteger(lua,2));
    return 0;
}

static int onGetMapTileWidthTiledEditorLua(lua_State * lua)
{
    mbm::TILE_EDITOR * tileEditor  = getTileEditorFromRawTable(lua,1,1);
    lua_pushinteger(lua,tileEditor->getMapTileWidth());
    return 1;
}
static int onSetMapTileWidthTiledEditorLua(lua_State * lua)
{
    mbm::TILE_EDITOR * tileEditor  = getTileEditorFromRawTable(lua,1,1);
    tileEditor->setMapTileWidth(luaL_checkinteger(lua,2));
    return 0;
}
static int onGetMapTileHeightTiledEditorLua(lua_State * lua)
{
    mbm::TILE_EDITOR * tileEditor  = getTileEditorFromRawTable(lua,1,1);
    lua_pushinteger(lua,tileEditor->getMapTileHeight());
    return 1;
}
static int onSetMapTileHeightTiledEditorLua(lua_State * lua)
{
    mbm::TILE_EDITOR * tileEditor  = getTileEditorFromRawTable(lua,1,1);
    tileEditor->setMapTileHeight(luaL_checkinteger(lua,2));
    return 0;
}

static int onGetTileSetWidthTiledEditorLua(lua_State * lua)
{
    mbm::TILE_EDITOR * tileEditor  = getTileEditorFromRawTable(lua,1,1);
    const uint32_t index           = luaL_checkinteger(lua,2);
    lua_pushinteger(lua,tileEditor->getTileSetWidth(index-1));
    return 1;
}
static int onSetTileSetWidthTiledEditorLua(lua_State * lua)
{
    mbm::TILE_EDITOR * tileEditor  = getTileEditorFromRawTable(lua,1,1);
    const uint32_t index           = luaL_checkinteger(lua,2);
    tileEditor->setTileSetWidth(index-1,luaL_checkinteger(lua,3));
    return 0;
}
static int onGetTileSetHeightTiledEditorLua(lua_State * lua)
{
    mbm::TILE_EDITOR * tileEditor  = getTileEditorFromRawTable(lua,1,1);
    const uint32_t index           = luaL_checkinteger(lua,2);
    lua_pushinteger(lua,tileEditor->getTileSetHeight(index-1));
    return 1;
}
static int onSetTileSetHeightTiledEditorLua(lua_State * lua)
{
    mbm::TILE_EDITOR * tileEditor  = getTileEditorFromRawTable(lua,1,1);
    const uint32_t index           = luaL_checkinteger(lua,2);
    tileEditor->setTileSetHeight(index-1,luaL_checkinteger(lua,3));
    return 0;
}

static int onExistTileSetTiledEditorLua(lua_State * lua)
{
    mbm::TILE_EDITOR * tileEditor  = getTileEditorFromRawTable(lua,1,1);
    const char* name               = luaL_checkstring(lua,2);
    lua_pushboolean(lua,tileEditor->existTileSet(name));
    return 1;
}

static int onSetTileSetNameTiledEditorLua(lua_State * lua)
{
    mbm::TILE_EDITOR * tileEditor  = getTileEditorFromRawTable(lua,1,1);
    const uint32_t index           = luaL_checkinteger(lua,2);
    const char* name               = luaL_checkstring(lua,3);
    tileEditor->setTileSetName(index-1, name);
    return 0;
}


static int onGetMapBackgroundColorTiledEditorLua(lua_State * lua)
{
    mbm::TILE_EDITOR * tileEditor  = getTileEditorFromRawTable(lua,1,1);
    lua_push_rgba(lua,tileEditor->getBackgroundColor());
    return 1;
}
static int onSetMapBackgroundColorTiledEditorLua(lua_State * lua)
{
    mbm::TILE_EDITOR * tileEditor  = getTileEditorFromRawTable(lua,1,1);
    auto color = lua_pop_rgba(lua,2);
    tileEditor->setBackgroundColor(color);
    return 0;
}
static int onGetMapBackgroundTextureTiledEditorLua(lua_State * lua)
{
    mbm::TILE_EDITOR * tileEditor  = getTileEditorFromRawTable(lua,1,1);
    const char * texture_file_name = tileEditor->getBackgroundTexture();
    if(texture_file_name)
        lua_pushstring(lua,texture_file_name);
    else
        lua_pushstring(lua, "");
    return 1;
}
static int onSetMapBackgroundTextureTiledEditorLua(lua_State * lua)
{
    mbm::TILE_EDITOR * tileEditor  = getTileEditorFromRawTable(lua,1,1);
    const char * texture_name      = lua_type(lua,2) == LUA_TSTRING ? lua_tostring(lua,2) : nullptr;
    tileEditor->setBackgroundTexture(texture_name);
    return 0;
}
static int onGetMapPropertiesTiledEditorLua(lua_State * lua)
{
    mbm::TILE_EDITOR * tileEditor  = getTileEditorFromRawTable(lua,1,1);
    auto & properties =   tileEditor->getMapProperties();
    lua_push_properties(lua,properties);
    return 1;
}

static int onSetMapPropertiesTiledEditorLua(lua_State * lua)
{
    mbm::TILE_EDITOR * tileEditor  = getTileEditorFromRawTable(lua,1,1);
    auto & properties =   tileEditor->getMapProperties();
    lua_set_properties(lua,2,properties);
    return 0;
}

static int onGetNameShaderLayerTiledEditorLua(lua_State * lua)
{
    mbm::TILE_EDITOR * tileEditor  = getTileEditorFromRawTable(lua,1,1);
    uint32_t index                 = luaL_checkinteger(lua,2);
    if(index == 0 )
    {
        plugin_helper::lua_error_debug(lua,"expected index one based. Got [%d]",index);
    }
    index--;
    lua_pushstring(lua,tileEditor->getNamePsShaderLayer(index));
    return 1;
}

static int onGetLayerTiledEditorLua(lua_State * lua)
{
    mbm::TILE_EDITOR * tileEditor  = getTileEditorFromRawTable(lua,1,1);
    uint32_t index                 = luaL_checkinteger(lua,2);
    if(index == 0 )
    {
        plugin_helper::lua_error_debug(lua,"expected index one based. Got [%d]",index);
    }
    index--;
    if(index >= tileEditor->getTotalLayer())
    {
        plugin_helper::lua_error_debug(lua,"index one based out of range for layer. Got [%d]",index);
    }
    lua_newtable(lua);

    lua_pushboolean(lua,tileEditor->isLayerVisible(index));
    lua_setfield(lua,-2,"visible");

    lua_push_rgba(lua,tileEditor->getMinTintLayer(index));
    lua_setfield(lua,-2,"minTint");

    lua_push_rgba(lua,tileEditor->getMaxTintLayer(index));
    lua_setfield(lua,-2,"maxTint");

    lua_push_vec2(lua,tileEditor->getOffsetLayer(index));
    lua_setfield(lua,-2,"offset");

    lua_pushinteger(lua,((unsigned int)tileEditor->getTintAnimTypeLayer(index)) + 1);
    lua_setfield(lua,-2,"tintAnimType");

    lua_pushnumber(lua,tileEditor->getTintAnimTimeLayer(index));
    lua_setfield(lua,-2,"tintAnimTime");

    return 1;
}

static int onNewLayerTiledEditorLua(lua_State * lua)
{
    mbm::TILE_EDITOR * tileEditor  = getTileEditorFromRawTable(lua,1,1);
    lua_pushboolean(lua,tileEditor->addLayer());
    return 0;
}

static int onUpdateLayerTiledEditorLua(lua_State * lua)
{
    mbm::TILE_EDITOR * tileEditor  = getTileEditorFromRawTable(lua,1,1);
    uint32_t index                 = luaL_checkinteger(lua,2);
    lua_check_is_table(lua, 3, "{visible,minTint,maxTint,offset,tintAnimType,tintAnimTime}");
    if(index == 0 )
    {
        plugin_helper::lua_error_debug(lua,"expected index one based. Got [%d]",index);
    }
    index--;
    
    if (tileEditor->existLayer(index))
    {
        lua_getfield(lua, 3, "visible");
        tileEditor->setLayerVisible(index,lua_toboolean(lua,-1));
        lua_pop(lua, 1);

        lua_getfield(lua, 3, "offset");
        tileEditor->setOffsetLayer(index,lua_pop_vec2(lua,lua_gettop(lua)));
        lua_pop(lua, 1);

        lua_getfield(lua, 3, "minTint");
        tileEditor->setMinTintLayer(index,lua_pop_rgba(lua,lua_gettop(lua)));
        lua_pop(lua, 1);

        lua_getfield(lua, 3, "maxTint");
        tileEditor->setMaxTintLayer(index,lua_pop_rgba(lua,lua_gettop(lua)));
        lua_pop(lua, 1);

        lua_getfield(lua, 3, "tintAnimType");
        mbm::TYPE_ANIMATION type;
        unsigned int v = lua_tointeger(lua,lua_gettop(lua));
        switch(v)
        {
            case 1 : type = mbm::TYPE_ANIMATION_PAUSED;          break;
            case 2 : type = mbm::TYPE_ANIMATION_GROWING;         break;
            case 3 : type = mbm::TYPE_ANIMATION_GROWING_LOOP;    break;
            case 4 : type = mbm::TYPE_ANIMATION_DECREASING;      break;
            case 5 : type = mbm::TYPE_ANIMATION_DECREASING_LOOP; break;
            case 6 : type = mbm::TYPE_ANIMATION_RECURSIVE;       break;
            case 7 : type = mbm::TYPE_ANIMATION_RECURSIVE_LOOP;  break;
            default: type = mbm::TYPE_ANIMATION_PAUSED;
        }
        tileEditor->setTintAnimTypeLayer(index,type);
        lua_pop(lua, 1);

        lua_getfield(lua, 3, "tintAnimTime");
        const float time = lua_tonumber(lua,lua_gettop(lua));
        tileEditor->setTintAnimTimeLayer(index,time);
        lua_pop(lua, 1);
        
        lua_pushboolean(lua,1);
    }
    else
    {
        lua_pushboolean(lua,0);
    }

    return 1;
}


static int onDeleteLayerTiledEditorLua(lua_State * lua)
{
    mbm::TILE_EDITOR * tileEditor  = getTileEditorFromRawTable(lua,1,1);
    uint32_t index                 = luaL_checkinteger(lua,2);
    if(index == 0 )
    {
        plugin_helper::lua_error_debug(lua,"expected index one based. Got [%d]",index);
    }
    index--;
    tileEditor->eraseLayer(index);
    return 0;
}

static int onGetLayerPropertiesTiledEditorLua(lua_State * lua)
{
    mbm::TILE_EDITOR * tileEditor  = getTileEditorFromRawTable(lua,1,1);
    uint32_t index                 = luaL_checkinteger(lua,2);
    if(index == 0 )
    {
        plugin_helper::lua_error_debug(lua,"expected index one based. Got [%d]",index);
    }
    index--;
    if (tileEditor->existLayer(index))
    {
        auto & properties = tileEditor->getLayerProperties(index);
        lua_push_properties(lua,properties);
    }
    else
    {
        plugin_helper::lua_error_debug(lua,"Layer [%d] not found",index);
        lua_pushnil(lua);
    }
    return 1;
}


static int onSetLayerPropertiesTiledEditorLua(lua_State * lua)
{
    mbm::TILE_EDITOR * tileEditor  = getTileEditorFromRawTable(lua,1,1);
    uint32_t index                 = luaL_checkinteger(lua,2);
    if(index == 0 )
    {
        plugin_helper::lua_error_debug(lua,"expected index one based. Got [%d]",index);
    }
    index--;
    if (tileEditor->existLayer(index))
    {
        auto & properties = tileEditor->getLayerProperties(index);
        lua_set_properties(lua,3,properties);
    }
    else
    {
        plugin_helper::lua_error_debug(lua,"Layer [%d] not found",index+1);
    }
    return 0;
}

static int onGetBrickPropertiesTiledEditorLua(lua_State * lua)
{
    mbm::TILE_EDITOR * tileEditor  = getTileEditorFromRawTable(lua,1,1);
    uint32_t index                 = luaL_checkinteger(lua,2);
    auto brick                     = tileEditor->getBrick(index,nullptr);
    if(brick)
    {
        auto & properties = brick->getProperties();
        lua_push_properties(lua,properties);
    }
    else
    {
        plugin_helper::lua_error_debug(lua,"Not found brick for Brick id [%u] filter [none] ",index);
    }
    return 1;
}

static int onGetBrickIDByTileIDLua(lua_State * lua)
{
    mbm::TILE_EDITOR * tileEditor  = getTileEditorFromRawTable(lua,1,1);
    uint32_t index                 = luaL_checkinteger(lua,2)-1;
    uint16_t id                    = tileEditor->getBrickIDByTileID(index);
    if(id > (tileEditor->getMapCountWidth() * tileEditor->getMapCountHeight()))
        id = 0;
    lua_pushinteger(lua,id);
    return 1;
}

static int onSetBrickPropertiesTiledEditorLua(lua_State * lua)
{
    mbm::TILE_EDITOR * tileEditor  = getTileEditorFromRawTable(lua,1,1);
    uint32_t index                 = luaL_checkinteger(lua,2);
    auto brick                     = tileEditor->getBrick(index,nullptr);
    if(brick)
    {
        auto & properties = brick->getProperties();
        lua_set_properties(lua,3,properties);
    }
    else
    {
        plugin_helper::lua_error_debug(lua,"Not found brick for Brick id [%u] filter [none] ",index);
    }
    return 0;
}

static int onNewTileSetTiledEditorLua(lua_State * lua)
{
    const int top                  = lua_gettop(lua);
    mbm::TILE_EDITOR * tileEditor  = getTileEditorFromRawTable(lua,1,1);
    std::vector<std::string> files;
    switch(lua_type(lua,2))
    {
        case LUA_TTABLE:
        {
            const int s = lua_rawlen(lua,2);
            for(int i = 0; i < s; ++i)
            {
                lua_rawgeti(lua,2,i+1);
                const char * texture_file_name  = luaL_checkstring(lua,-1);
                files.push_back(texture_file_name);
                lua_pop(lua,1);
            }
        }
        break;
        case LUA_TSTRING:
        {
            const char * texture_file_name  = luaL_checkstring(lua,2);
            files.push_back(texture_file_name);
        }
        break;
        default:
        {
            plugin_helper::lua_error_debug(lua,"Expected file_name of texture to the tileset");
        }
    }
    mbm::VEC2 min_bound;
    mbm::VEC2 max_bound;
    const char * name              = luaL_checkstring(lua,3);
    const uint32_t width           = luaL_checkinteger(lua,4);
    const uint32_t height          = luaL_checkinteger(lua,5);
    const uint32_t space_x         = luaL_checkinteger(lua,6);
    const uint32_t space_y         = luaL_checkinteger(lua,7);
    const uint32_t margin_x        = luaL_checkinteger(lua,8);
    const uint32_t margin_y        = luaL_checkinteger(lua,9);
    min_bound.x                    = top == 13 ? luaL_checkinteger(lua,10) : 0.0f;
    min_bound.y                    = top == 13 ? luaL_checkinteger(lua,11) : 0.0f;
    max_bound.x                    = top == 13 ? luaL_checkinteger(lua,12) : 0.0f;
    max_bound.y                    = top == 13 ? luaL_checkinteger(lua,13) : 0.0f;
    const bool ret = tileEditor->newTileSet(name,files,width,height,space_x,space_y,margin_x,margin_y,top == 13 ? &min_bound : nullptr, top == 13 ? &max_bound : nullptr);
    lua_pushboolean(lua,ret ? 1 : 0);
    return 1;
}

static int onGetTotalTileSetTiledEditorLua(lua_State * lua)
{
    mbm::TILE_EDITOR * tileEditor  = getTileEditorFromRawTable(lua,1,1);
    lua_pushinteger(lua,tileEditor->getTotalTileSet());
    return 1;
}

static int onGetTileSetNameTiledEditorLua(lua_State * lua)
{
    mbm::TILE_EDITOR * tileEditor  = getTileEditorFromRawTable(lua,1,1);
    const uint32_t index           = luaL_checkinteger(lua,2);
    if(index == 0)
        plugin_helper::lua_error_debug(lua,"Expected index one based ");
    else if( index > tileEditor->getTotalTileSet())
        plugin_helper::lua_error_debug(lua,"Index major then the availables tileset [%u/%u]",index,tileEditor->getTotalTileSet());
    else
        lua_pushstring(lua,tileEditor->getTileSetName(index-1));
    return 1;
}


static int onSetRenderModeTiledEditorLua(lua_State * lua)
{
    mbm::TILE_EDITOR * tileEditor  = getTileEditorFromRawTable(lua,1,1);
    const char * name              = luaL_checkstring(lua,2);
    switch (tileEditor->render_what)
    {
        case mbm::RENDER_MAP:
        {
            tileEditor->cam_map_pos  =  tileEditor->device->camera.position2d;
        }
        break;
        case mbm::RENDER_LAYER:
        {
            tileEditor->cam_layer_pos =  tileEditor->device->camera.position2d;
        }
        break;
        case mbm::RENDER_TILE_SET:
        {
            tileEditor->cam_tileset_pos =  tileEditor->device->camera.position2d;
        }
        break;
        default:{}
    }
    if(strcmp(name,"tileset") == 0)
    {
        tileEditor->render_what       = mbm::RENDER_TILE_SET;
        tileEditor->index_render_what = luaL_checkinteger(lua,3) - 1;
        if(tileEditor->index_render_what >= tileEditor->getTotalTileSet())
            tileEditor->index_render_what = 0;
        lua_pushinteger(lua,tileEditor->index_render_what + 1);
        tileEditor->device->camera.position2d = tileEditor->cam_tileset_pos;
    }
    else if(strcmp(name,"map") == 0)
    {
        tileEditor->render_what       = mbm::RENDER_MAP;
        tileEditor->index_render_what = 0;
        lua_pushinteger(lua,tileEditor->index_render_what);
        tileEditor->device->camera.position2d = tileEditor->cam_map_pos;
    }
    else if(strcmp(name,"layer") == 0)
    {
        tileEditor->render_what       = mbm::RENDER_LAYER;
        tileEditor->index_render_what = luaL_checkinteger(lua,3) - 1;
        if(tileEditor->index_render_what >= tileEditor->getTotalLayer())
            tileEditor->index_render_what = 0;
        lua_pushinteger(lua,tileEditor->index_render_what+1);
        tileEditor->device->camera.position2d = tileEditor->cam_layer_pos;
    }
    else if(strcmp(name,"brick") == 0)
    {
        tileEditor->render_what       = mbm::RENDER_BRICK;
        tileEditor->index_render_what = luaL_checkinteger(lua,3);
        if(tileEditor->index_render_what > tileEditor->getTotalBricks(nullptr))
            tileEditor->index_render_what = 1;
        lua_pushinteger(lua,tileEditor->index_render_what);
        tileEditor->device->camera.position2d.x = 0;
        tileEditor->device->camera.position2d.y = 0;
    }
    
    return 1;
}

static int onGetTotalBricksTiledEditorLua(lua_State * lua)
{
    const int top = lua_gettop(lua);
    mbm::TILE_EDITOR * tileEditor  = getTileEditorFromRawTable(lua,1,1);
    const char * filterTileSet     = top > 1 ? luaL_checkstring(lua,2) : nullptr;
    lua_pushinteger(lua,tileEditor->getTotalBricks(filterTileSet));
    return 1;
}

void lua_push_brick(lua_State * lua,const std::shared_ptr<mbm::BRICK> & brick)
{
    lua_newtable(lua);
    lua_pushinteger(lua,brick->id);
    lua_setfield(lua,-2,"id");

    lua_pushinteger(lua,brick->width);
    lua_setfield(lua,-2,"width");

    lua_pushinteger(lua,brick->height);
    lua_setfield(lua,-2,"height");

    if(brick->texture)
        lua_pushstring(lua,brick->texture->getFileNameTexture());
    else
        lua_pushstring(lua,"");
    lua_setfield(lua,-2,"texture");

    lua_newtable(lua);
    for( int i=0; i <4; ++i)
    {
        lua_push_vec2(lua,brick->uv[i]);
        lua_rawseti(lua,-2, i + 1);
    }
    lua_setfield(lua,-2,"uv");
}

void lua_update_brick(lua_State * lua,std::shared_ptr<mbm::BRICK> & brick,const int index)
{
    lua_check_is_table(lua, index, "{id, width,height,texture,uv = {}}");
    lua_getfield(lua, index, "id");
    const uint32_t id = luaL_checkinteger(lua,-1);
    lua_pop(lua, 1);
    if(brick == nullptr || id != brick->id)
    {
        plugin_helper::lua_error_debug(lua,"Brick Id expected [%u] got [%u]",brick != nullptr ? brick->id : 0,id);
    }

    brick->backup();

    lua_getfield(lua, index, "width");
    brick->width = luaL_checkinteger(lua,-1);
    lua_pop(lua, 1);

    lua_getfield(lua, index, "height");
    brick->height = luaL_checkinteger(lua,-1);
    lua_pop(lua, 1);

    lua_getfield(lua, index, "texture");
    const char * texture_name = luaL_checkstring(lua,-1);
    lua_pop(lua, 1);

    if(texture_name && strlen(texture_name) > 0 )
    {
        mbm::TEXTURE::enableFilter(false);
        mbm::TEXTURE * texture     = mbm::TEXTURE_MANAGER::getInstance()->load(texture_name,true);
        mbm::TEXTURE::enableFilter(true);
        if(texture)
        {
            brick->setTexture(texture);
        }
    }
    else
    {
        brick->setTexture(nullptr);
    }

    lua_getfield(lua, index, "uv");
    for(int i=0; i < 4; ++i)
    {
        lua_rawgeti(lua,-1,i+1);
        lua_getfield(lua, -1, "x");
        brick->uv[i].x = static_cast<float>(luaL_checknumber(lua,-1));
        lua_pop(lua, 1);

        lua_getfield(lua, -1, "y");
        brick->uv[i].y = static_cast<float>(luaL_checknumber(lua,-1));
        lua_pop(lua, 2);
    }
    brick->build(nullptr,brick->uv);
}

//this method works with absolute index when using filter (stored at vector). Use id (map) when no filter . (different place to search)
static int onGetBrickTiledEditorLua(lua_State * lua)
{
    const int top                  = lua_gettop(lua);
    mbm::TILE_EDITOR * tileEditor  = getTileEditorFromRawTable(lua,1,1);
    uint32_t index                 = luaL_checkinteger(lua,2);
    const char * filterTileSet     = top > 2 ? luaL_checkstring(lua,3) : nullptr;
    if(filterTileSet)
        index = index - 1;
    auto brick                     = tileEditor->getBrick(index,filterTileSet);
    if(brick == nullptr)
    {
        if(filterTileSet)
            plugin_helper::lua_error_debug(lua,"Not found brick for absolute index [%u] filter [%s] ",index, filterTileSet);
        else
            plugin_helper::lua_error_debug(lua,"Not found brick for Brick id [%u] filter [none] ",index);
    }
    else
        lua_push_brick(lua,brick);
    return 1;
}

static int onUpdateBrickTiledEditorLua(lua_State * lua)
{
    mbm::TILE_EDITOR * tileEditor  = getTileEditorFromRawTable(lua,1,1);
    const uint32_t id              = luaL_checkinteger(lua,2);
    auto brick                     = tileEditor->getBrick(id,nullptr);
    if(brick == nullptr)
        plugin_helper::lua_error_debug(lua,"id [%u] out of range ",id);
    else
        lua_update_brick(lua,brick,3);
    return 1;
}

static int onUndoChangesBrickTiledEditorLua(lua_State * lua)
{
    mbm::TILE_EDITOR * tileEditor  = getTileEditorFromRawTable(lua,1,1);
    const uint32_t id              = luaL_checkinteger(lua,2);
    auto brick                     = tileEditor->getBrick(id,nullptr);
    if(brick == nullptr)
        plugin_helper::lua_error_debug(lua,"index [%u] out of range ",id);
    else
    {
        brick->restore_backup();
        brick->build(nullptr,brick->uv);
    }
    return 0;
}

static int onExpandBrickTiledEditorLua(lua_State * lua)
{
    mbm::TILE_EDITOR * tileEditor  = getTileEditorFromRawTable(lua,1,1);
    const uint32_t id              = luaL_checkinteger(lua,2);
    auto brick                     = tileEditor->getBrick(id,nullptr);
    if(brick == nullptr)
        plugin_helper::lua_error_debug(lua,"id  [%u] out of range ",id);
    else
    {
        const bool inside          = lua_toboolean(lua,3);
        const float value          = luaL_checknumber(lua,4);
        brick->backup();
        brick->expand(inside,value);
        brick->build(nullptr,brick->uv);
    }
    return 0;
}

static int onExpandVBrickTiledEditorLua(lua_State * lua)
{
    mbm::TILE_EDITOR * tileEditor  = getTileEditorFromRawTable(lua,1,1);
    const uint32_t id              = luaL_checkinteger(lua,2);
    auto brick                     = tileEditor->getBrick(id,nullptr);
    if(brick == nullptr)
        plugin_helper::lua_error_debug(lua,"id  [%u] out of range ",id);
    else
    {
        const bool inside          = lua_toboolean(lua,3);
        const float value          = luaL_checknumber(lua,4);
        brick->backup();
        brick->expandV(inside,value);
        brick->build(nullptr,brick->uv);
    }
    return 0;
}

static int onExpandHBrickTiledEditorLua(lua_State * lua)
{
    mbm::TILE_EDITOR * tileEditor  = getTileEditorFromRawTable(lua,1,1);
    const uint32_t id              = luaL_checkinteger(lua,2);
    auto brick                     = tileEditor->getBrick(id,nullptr);
    if(brick == nullptr)
        plugin_helper::lua_error_debug(lua,"id  [%u] out of range ",id);
    else
    {
        const bool inside          = lua_toboolean(lua,3);
        const float value          = luaL_checknumber(lua,4);
        brick->backup();
        brick->expandH(inside,value);
        brick->build(nullptr,brick->uv);
    }
    return 0;
}

std::shared_ptr<mbm::CUBE> lua_pop_cube(lua_State * lua, int index)
{
    std::shared_ptr<mbm::CUBE> cube = std::make_shared<mbm::CUBE>();
    lua_check_is_table(lua, index, "{type = 'rectangle', width,height,x,y }");
    
    lua_getfield(lua, index, "width");
    cube->halfDim.x = luaL_checknumber(lua,-1) * 0.5f;
    lua_pop(lua, 1);

    lua_getfield(lua, index, "height");
    cube->halfDim.y = luaL_checknumber(lua,-1) * 0.5f;
    lua_pop(lua, 1);

    lua_getfield(lua, index, "x");
    cube->absCenter.x = luaL_checknumber(lua,-1);
    lua_pop(lua, 1);

    lua_getfield(lua, index, "y");
    cube->absCenter.y = luaL_checknumber(lua,-1);
    lua_pop(lua, 1);
    return cube;
}

std::shared_ptr<mbm::SPHERE> lua_pop_sphere(lua_State * lua, int index)
{
    std::shared_ptr<mbm::SPHERE> sphere = std::make_shared<mbm::SPHERE>();
    lua_check_is_table(lua, index, "{type = 'circle', ray,x,y }");
    
    lua_getfield(lua, index, "ray");
    sphere->ray = luaL_checknumber(lua,-1);
    lua_pop(lua, 1);

    lua_getfield(lua, index, "x");
    sphere->absCenter[0] = luaL_checknumber(lua,-1);
    lua_pop(lua, 1);

    lua_getfield(lua, index, "y");
    sphere->absCenter[1] = luaL_checknumber(lua,-1);
    lua_pop(lua, 1);

    return sphere;
}

std::shared_ptr<mbm::TRIANGLE> lua_pop_triangle(lua_State * lua, int index)
{
    std::shared_ptr<mbm::TRIANGLE> triangle = std::make_shared<mbm::TRIANGLE>();
    lua_check_is_table(lua, index, "{type = 'triangle', p1 = {x,y}, p2 = {x,y}, p3 = {x,y} }");

    lua_getfield(lua, index, "x");
    triangle->position.x = luaL_checknumber(lua,-1);
    lua_pop(lua, 1);

    lua_getfield(lua, index, "y");
    triangle->position.y = luaL_checknumber(lua,-1);
    lua_pop(lua, 1);
    
    for(int i=0; i < 3; i++)
    {
        char pos[3] = {'p', 0, 0};
        pos[1] = '1' + i;
        lua_getfield(lua, index, pos);

        lua_getfield(lua, -1, "x");
        triangle->point[i].x = luaL_checknumber(lua,-1);
        lua_pop(lua, 1);

        lua_getfield(lua, -1, "y");
        triangle->point[i].y = luaL_checknumber(lua,-1);
        lua_pop(lua, 1);

        lua_pop(lua, 1);
    }
    return triangle;
}

void push_cube(lua_State * lua,const std::shared_ptr<mbm::CUBE> & cube)
{
    lua_newtable(lua);
    lua_pushstring(lua,"rectangle");
    lua_setfield(lua,-2,"type");

    lua_pushnumber(lua,cube->halfDim.x * 2);
    lua_setfield(lua,-2,"width");

    lua_pushnumber(lua,cube->halfDim.y * 2);
    lua_setfield(lua,-2,"height");

    lua_pushnumber(lua,cube->absCenter.x);
    lua_setfield(lua,-2,"x");

    lua_pushnumber(lua,cube->absCenter.y);
    lua_setfield(lua,-2,"y");
}

void push_sphere(lua_State * lua,const std::shared_ptr<mbm::SPHERE> & sphere)
{
    lua_newtable(lua);
    lua_pushstring(lua,"circle");
    lua_setfield(lua,-2,"type");

    lua_pushnumber(lua,sphere->ray);
    lua_setfield(lua,-2,"ray");

    lua_pushnumber(lua,sphere->absCenter[0]);
    lua_setfield(lua,-2,"x");

    lua_pushnumber(lua,sphere->absCenter[1]);
    lua_setfield(lua,-2,"y");
}

void lua_push_lines(lua_State * lua,const std::shared_ptr<mbm::VEC2> line,const uint32_t size_lines)
{
    lua_newtable(lua);
    lua_pushstring(lua,"line");
    lua_setfield(lua,-2,"type");
    int index = 1;
    mbm::VEC2* vec = line.get();
    for(uint32_t i = 0;  i < size_lines; ++i)
    {
        auto & v = vec[i];
        lua_pushnumber(lua,v.x);
        lua_rawseti(lua,-2,index++);

        lua_pushnumber(lua,v.y);
        lua_rawseti(lua,-2,index++);
    }
}

void push_triangle(lua_State * lua,const std::shared_ptr<mbm::TRIANGLE> & triangle)
{
    lua_newtable(lua);
    lua_pushstring(lua,"triangle");
    lua_setfield(lua,-2,"type");

    lua_pushnumber(lua,triangle->position.x);
    lua_setfield(lua,-2,"x");

    lua_pushnumber(lua,triangle->position.y);
    lua_setfield(lua,-2,"y");

    for (int i=0; i < 3; ++i)
    {
        lua_newtable(lua);
        lua_pushnumber(lua,triangle->point[i].x);
        lua_setfield(lua,-2,"x");

        lua_pushnumber(lua,triangle->point[i].y);
        lua_setfield(lua,-2,"y");

        char pos[3] = {'p', 0, 0};
        pos[1] = '1' + i;
        lua_setfield(lua,-2,pos);
    }
}

static int onGetPhysicsBrickTiledEditorLua(lua_State * lua)
{
    mbm::TILE_EDITOR * tileEditor  = getTileEditorFromRawTable(lua,1,1);
    const uint32_t id              = luaL_checkinteger(lua,2);
    auto brick                     = tileEditor->getBrick(id,nullptr);
    if(brick == nullptr)
        plugin_helper::lua_error_debug(lua,"id  [%u] out of range ",id);
    else
    {
        int index = 1;
        lua_newtable(lua);
        lua_pushstring(lua,brick->brick_physics.name.c_str());
        lua_setfield(lua,-2,"name");
        for(uint32_t i=0; i < brick->brick_physics.lsCube.size(); ++i)
        {
            const auto & cube = brick->brick_physics.lsCube[i];
            push_cube(lua,cube);
            lua_rawseti(lua,-2, index++);
        }

        for(uint32_t i=0; i < brick->brick_physics.lsSphere.size(); ++i)
        {
            const auto & sphere = brick->brick_physics.lsSphere[i];
            push_sphere(lua,sphere);
            lua_rawseti(lua,-2, index++);
        }

        for(uint32_t i=0; i < brick->brick_physics.lsTriangle.size(); ++i)
        {
            const auto & triangle = brick->brick_physics.lsTriangle[i];
            push_triangle(lua,triangle);
            lua_rawseti(lua,-2, index++);
        }
    }
    return 1;
}

static int onSetPhysicsBrickTiledEditorLua(lua_State * lua)
{
    mbm::TILE_EDITOR * tileEditor  = getTileEditorFromRawTable(lua,1,1);
    const uint32_t id              = luaL_checkinteger(lua,2);
    auto brick                     = tileEditor->getBrick(id,nullptr);
    if(brick == nullptr)
        plugin_helper::lua_error_debug(lua,"id  [%u] out of range ",id);
    else
    {
        mbm::FREE_PHYSICS freePhysics;
        auto len = lua_rawlen(lua,3);
        for(size_t i=1; i <= len; ++i)
        {
            lua_rawgeti(lua,-1,i);
            if(lua_type(lua,-1) == LUA_TTABLE)
            {
                lua_getfield(lua,-1,"type");
                const char * type = luaL_checkstring(lua,-1);
                lua_pop(lua, 1);
                if(strcmp(type,"rectangle") == 0)
                {
                    auto cube = lua_pop_cube(lua,lua_gettop(lua));
                    freePhysics.lsCube.emplace_back(cube);
                }
                else if(strcmp(type,"circle") == 0)
                {
                    auto sphere = lua_pop_sphere(lua,lua_gettop(lua));
                    freePhysics.lsSphere.emplace_back(sphere);
                }
                else if(strcmp(type,"triangle") == 0)
                {
                    auto triangle = lua_pop_triangle(lua,lua_gettop(lua));
                    freePhysics.lsTriangle.emplace_back(triangle);
                }
            }
            else
            {
                plugin_helper::lua_error_debug(lua,"expected array of table to physics");
            }
            lua_pop(lua,1);
        }
        brick->brick_physics = std::move(freePhysics);
    }
    return 0;
}

static int onDeleteObjectMapTiledEditorLua(lua_State * lua)
{
    mbm::TILE_EDITOR * tileEditor  = getTileEditorFromRawTable(lua,1,1);
    const uint32_t index_object    = luaL_checkinteger(lua,2);
    auto & objects                 = tileEditor->getObjectsMap();
    if(index_object > 0 && index_object <= objects.size())
    {
        objects.erase(objects.begin() + (index_object - 1));
    }
    else
    {
        plugin_helper::lua_error_debug(lua,"Index [%u] out of range [%u]",index_object,objects.size());
    }
    return 0;
}


static int onGetTotalObjectMapTiledEditorLua(lua_State * lua)
{
    mbm::TILE_EDITOR * tileEditor  = getTileEditorFromRawTable(lua,1,1);
    auto & objects                 = tileEditor->getObjectsMap();
    lua_pushinteger(lua,objects.size());
    return 1;
}

static int onAddObjectMapTiledEditorLua(lua_State * lua)
{
    mbm::TILE_EDITOR * tileEditor  = getTileEditorFromRawTable(lua,1,1);
    auto & objects                 = tileEditor->getObjectsMap();
    if(lua_type(lua,2) == LUA_TTABLE)
    {
        lua_getfield(lua,-1,"type");
        const char * type = luaL_checkstring(lua,-1);
        lua_pop(lua, 1);

        lua_getfield(lua,-1,"name");
        const char * name = luaL_checkstring(lua,-1);
        lua_pop(lua, 1);
        auto obj  = std::make_shared<mbm::OBJ_TILE_MAP>();
        if(strcmp(type,"rectangle") == 0)
        {
            auto cube = lua_pop_cube(lua,-1);
            obj->cube = std::move(cube);
            objects.emplace_back(obj);
        }
        else if(strcmp(type,"circle") == 0)
        {
            auto sphere = lua_pop_sphere(lua,-1);
            obj->sphere = std::move(sphere);
            objects.emplace_back(obj);
        }
        else if(strcmp(type,"triangle") == 0)
        {
            auto triangle = lua_pop_triangle(lua,-1);
            obj->triangle = std::move(triangle);
            objects.emplace_back(obj);
        }
        if(objects.size())
            objects[objects.size()-1]->name = name;
    }
    else
    {
        plugin_helper::lua_error_debug(lua,"Expected <table {type = rectangle,triangle,circle,point,line}>");
    }
    return 0;
}

static int onGetObjectMapTiledEditorLua(lua_State * lua)
{
    mbm::TILE_EDITOR * tileEditor  = getTileEditorFromRawTable(lua,1,1);
    const uint32_t index_object    = luaL_checkinteger(lua,2);
    auto & objects                 = tileEditor->getObjectsMap();
    if(index_object > 0 && index_object <= objects.size())
    {
        auto & obj = objects[index_object - 1];
        if(obj->cube)
        {
            push_cube(lua,obj->cube);
        }
        else if(obj->triangle)
        {
            push_triangle(lua,obj->triangle);
        }
        else if(obj->sphere)
        {
            push_sphere(lua,obj->sphere);
        }
        else if(obj->point)
        {
            lua_push_vec2(lua,*obj->point);
            lua_pushstring(lua,"point");
            lua_setfield(lua,-2,"type");
        }
        else if(obj->line)
        {
            lua_push_lines(lua,obj->line,obj->size_line);
        }
        lua_pushstring(lua,obj->name.c_str());
        lua_setfield(lua,-2,"name");
    }
    else
    {
        plugin_helper::lua_error_debug(lua,"Index [%u] out of range [%u]",index_object,objects.size());
    }
    return 1;
}

static int onSetObjectMapTiledEditorLua(lua_State * lua)
{
    mbm::TILE_EDITOR * tileEditor  = getTileEditorFromRawTable(lua,1,1);
    uint32_t index_object          = luaL_checkinteger(lua,2);
    auto & objects                 = tileEditor->getObjectsMap();
    if(index_object > 0 && index_object <= objects.size())
    {
        index_object = index_object -1;
        if(lua_type(lua,3) == LUA_TTABLE)
        {
            lua_getfield(lua,-1,"type");
            const char * type = luaL_checkstring(lua,-1);
            lua_pop(lua, 1);

            lua_getfield(lua,-1,"name");
            const char * name = luaL_checkstring(lua,-1);
            lua_pop(lua, 1);

            objects[index_object]->release();
            objects[index_object]->name = name;
            if(strcmp(type,"rectangle") == 0)
            {
                auto cube = lua_pop_cube(lua,3);
                objects[index_object]->cube = std::move(cube);
            }
            else if(strcmp(type,"circle") == 0)
            {
                auto sphere = lua_pop_sphere(lua,3);
                objects[index_object]->sphere = std::move(sphere);
            }
            else if(strcmp(type,"triangle") == 0)
            {
                auto triangle = lua_pop_triangle(lua,3);
                objects[index_object]->triangle = std::move(triangle);
            }
            else if(strcmp(type,"point") == 0)
            {
                auto point = std::make_shared<mbm::VEC2>(lua_pop_vec2(lua,3));
                objects[index_object]->point = std::move(point);
            }
            else if(strcmp(type,"line") == 0)
            {
                uint32_t size_line = 0;
                auto line = lua_pop_line(lua,3,size_line);
                objects[index_object]->size_line = size_line;
                objects[index_object]->line = (line);
            }
        }
        else
        {
            plugin_helper::lua_error_debug(lua,"Expected <index> , <table {type = rectangle,triangle,circle,point,line}>");
        }
    }
    else
    {
        plugin_helper::lua_error_debug(lua,"Index [%u] out of range [%u]",index_object,objects.size());
    }
    return 0;
}



static int onGetScaleTiledEditorLua(lua_State * lua)
{
    mbm::TILE_EDITOR * tileEditor  = getTileEditorFromRawTable(lua,1,1);
    switch (tileEditor->render_what)
    {
        case mbm::RENDER_MAP:      lua_push_vec2(lua,tileEditor->scale_map);   break;
        case mbm::RENDER_TILE_SET: lua_push_vec2(lua,tileEditor->scale_tile);  break;
        case mbm::RENDER_LAYER:    lua_push_vec2(lua,tileEditor->scale_layer); break;
        case mbm::RENDER_BRICK:    lua_push_vec2(lua,tileEditor->scale_brick); break;
    }
    return 1;
}

static int onSetScaleTiledEditorLua(lua_State * lua)
{
    mbm::TILE_EDITOR * tileEditor  = getTileEditorFromRawTable(lua,1,1);
    switch (tileEditor->render_what)
    {
        case mbm::RENDER_MAP:      tileEditor->scale_map   = lua_pop_vec2(lua,2); break;
        case mbm::RENDER_TILE_SET: tileEditor->scale_tile  = lua_pop_vec2(lua,2); break;
        case mbm::RENDER_LAYER:    tileEditor->scale_layer = lua_pop_vec2(lua,2); break;
        case mbm::RENDER_BRICK:    tileEditor->scale_brick = lua_pop_vec2(lua,2); break;
    }
    return 0;
}

static int onGetTotalLayerTiledEditorLua(lua_State * lua)
{
    mbm::TILE_EDITOR * tileEditor  = getTileEditorFromRawTable(lua,1,1);
    lua_pushinteger(lua,tileEditor->getTotalLayer());
    return 1;
}

static int onOverBrickTiledEditorLua(lua_State * lua)
{
    mbm::TILE_EDITOR * tileEditor  = getTileEditorFromRawTable(lua,1,1);
    const float x = luaL_checknumber(lua,2);
    const float y = luaL_checknumber(lua,3);
    tileEditor->setOverBrick(x,y);
    return 0;
}

static int onSelectBrickTiledEditorLua(lua_State * lua)
{
    mbm::TILE_EDITOR * tileEditor  = getTileEditorFromRawTable(lua,1,1);
    const int top        = lua_gettop(lua);
    const uint32_t index = luaL_checkinteger(lua,2) - 1;
    const bool unique    = top > 2 ? lua_toboolean(lua,3): true;
    uint32_t iTotalSelected = tileEditor->selectBrick(index,unique);
    lua_pushinteger(lua,iTotalSelected);
    return 1;
}

static int onGetIndexOverBrickTiledEditorLua(lua_State * lua)
{
    mbm::TILE_EDITOR * tileEditor  = getTileEditorFromRawTable(lua,1,1);
    const int top = lua_gettop(lua);
    if(top > 1)
    {
        const float x = luaL_checknumber(lua,2);
        const float y = luaL_checknumber(lua,3);
        uint32_t index =  tileEditor->getIndexTileIdOver(x,y);
        if(index > (tileEditor->getMapCountWidth() * tileEditor->getMapCountHeight()))
            index = 0;
        else
            index = index + 1;
        lua_pushinteger(lua,index);
    }
    else
    {
        uint32_t index =  tileEditor->getLastBrickOver();
        if(index > (tileEditor->getMapCountWidth() * tileEditor->getMapCountHeight()))
            index = 0;
        else
            index = index + 1;
        lua_pushinteger(lua,index);
    }
    return 1;
}

static int onIsBrickSelectedTiledEditorLua(lua_State * lua)
{
    mbm::TILE_EDITOR * tileEditor  = getTileEditorFromRawTable(lua,1,1);
    const uint32_t index = luaL_checkinteger(lua,2) - 1;
    const bool value =  tileEditor->isBrickSelected(index);
    lua_pushboolean(lua,value);
    return 1;
}

static int onGetFirstSelectedBrickTiledEditorLua(lua_State * lua)
{
    mbm::TILE_EDITOR * tileEditor  = getTileEditorFromRawTable(lua,1,1);
    const uint16_t ID =  tileEditor->getFirstSelectedBrick();
    lua_pushinteger(lua,ID);
    return 1;
}

static int onUnselectBrickTiledEditorLua(lua_State * lua)
{
    mbm::TILE_EDITOR * tileEditor  = getTileEditorFromRawTable(lua,1,1);
    const uint32_t index = luaL_checkinteger(lua,2) - 1;
    tileEditor->unselectBrick(index);
    return 0;
}


static int onGetSelectedBrickTiledEditorLua(lua_State * lua)
{
    mbm::TILE_EDITOR * tileEditor  = getTileEditorFromRawTable(lua,1,1);
    const auto & selectedBricks    = tileEditor->getSelectedBrickMap();
    lua_newtable(lua);
    int index = 1;
    const auto total = tileEditor->getMapCountWidth() * tileEditor->getMapCountHeight();
    for (const auto & brick : selectedBricks)
    {
        if(brick.second && brick.first < total)
        {
            lua_pushinteger(lua,brick.first + 1);
            lua_rawseti(lua,-2, index++);
        }
    }
    return 1;
}


static int onSetBrick2LayerTiledEditorLua(lua_State * lua)
{
    const int top                  = lua_gettop(lua);
    mbm::TILE_EDITOR * tileEditor  = getTileEditorFromRawTable(lua,1,1);
    const uint32_t idBrick         = luaL_checkinteger(lua,2);
    const uint32_t indexLayer      = top > 2 ? luaL_checkinteger(lua,3) -1 : tileEditor->getLastBrickOver();
    const bool result              = tileEditor->setBrick2Layer(idBrick,indexLayer);
    lua_pushboolean(lua,result);
    return 1;
}

static int onSelectAllBricksTiledEditorLua(lua_State * lua)
{
    mbm::TILE_EDITOR * tileEditor  = getTileEditorFromRawTable(lua,1,1);
    tileEditor->selectAllBricks();
    return 0;
}

static int onInvertSelectedBricksTiledEditorLua(lua_State * lua)
{
    mbm::TILE_EDITOR * tileEditor  = getTileEditorFromRawTable(lua,1,1);
    tileEditor->invertSelectedBricks();
    return 0;
}

static int onUnSelectBricksTiledEditorLua(lua_State * lua)
{
    mbm::TILE_EDITOR * tileEditor  = getTileEditorFromRawTable(lua,1,1);
    tileEditor->unselectAllBricks();
    return 0;
}

static int onDeleteSelectedBricksTiledEditorLua(lua_State * lua)
{
    mbm::TILE_EDITOR * tileEditor  = getTileEditorFromRawTable(lua,1,1);
    tileEditor->deleteSelectedBricks();
    return 0;
}

static int onMoveLayerUpTiledEditorLua(lua_State * lua)
{
    mbm::TILE_EDITOR * tileEditor  = getTileEditorFromRawTable(lua,1,1);
    uint32_t index_layer           = luaL_checkinteger(lua,2);
    if(index_layer == 0 )
    {
        plugin_helper::lua_error_debug(lua,"expected index one based. Got [%d]",index_layer);
    }
    index_layer--;
    if(index_layer >= tileEditor->getTotalLayer())
    {
        plugin_helper::lua_error_debug(lua,"index one based out of range for layer. Got [%d]",index_layer +1);
    }
    tileEditor->moveLayerUp(index_layer);
    return 0;
}

static int onMoveLayerDownTiledEditorLua(lua_State * lua)
{
    mbm::TILE_EDITOR * tileEditor  = getTileEditorFromRawTable(lua,1,1);
    uint32_t index_layer           = luaL_checkinteger(lua,2);
    if(index_layer == 0 )
    {
        plugin_helper::lua_error_debug(lua,"expected index layer one based. Got [%d]",index_layer);
    }
    index_layer--;
    if(index_layer >= tileEditor->getTotalLayer())
    {
        plugin_helper::lua_error_debug(lua,"index layer one based out of range for layer. Got [%d]",index_layer +1);
    }
    tileEditor->moveLayerDown(index_layer);
    return 0;
}

static int onDuplicateSelectedTilesLua(lua_State * lua)
{
    mbm::TILE_EDITOR * tileEditor  = getTileEditorFromRawTable(lua,1,1);
    const float x                  = luaL_checknumber(lua,2);
    const float y                  = luaL_checknumber(lua,3);
    tileEditor->duplicateSelectedTiles(x,y);
    return 0;
}

static int onRotateSelectedBrickLayerTiledEditorLua(lua_State * lua)
{
    mbm::TILE_EDITOR * tileEditor  = getTileEditorFromRawTable(lua,1,1);
    const char* orientation = luaL_checkstring(lua,2);
    uint32_t rotated_id     = tileEditor->rotateSelectedBrick(orientation);
    lua_pushinteger(lua,rotated_id);
    return 1;
}

static int onFlipSelectedLayerTiledEditorLua(lua_State * lua)
{
    mbm::TILE_EDITOR * tileEditor  = getTileEditorFromRawTable(lua,1,1);
    uint32_t flipSelectedBrick     =  tileEditor->flipSelectedBrick();
    lua_pushinteger(lua,flipSelectedBrick);
    return 1;
}

static int onShowTileSetPreviewTiledEditorLua(lua_State * lua)
{
    mbm::TILE_EDITOR * tileEditor = getTileEditorFromRawTable(lua,1,1);
    const char* texture           = luaL_checkstring(lua,2);
    const int32_t width           = luaL_checkinteger(lua,3);
    const int32_t height          = luaL_checkinteger(lua,4);
    const int32_t space_x         = luaL_checkinteger(lua,5);
    const int32_t space_y         = luaL_checkinteger(lua,6);
    const int32_t margin_x        = luaL_checkinteger(lua,7);
    const int32_t margin_y        = luaL_checkinteger(lua,8);
    tileEditor->setTileSetPreview(texture,width,height,space_x,space_y,margin_x,margin_y);
    return 0;
}

static int onAddHistoricMapTiledEditorLua(lua_State * lua)
{
    mbm::TILE_EDITOR * tileEditor = getTileEditorFromRawTable(lua,1,1);
    tileEditor->addHistoric();
    return 0;
}

static int onUnDoMapTiledEditorLua(lua_State * lua)
{
    mbm::TILE_EDITOR * tileEditor = getTileEditorFromRawTable(lua,1,1);
    lua_pushboolean(lua,tileEditor->unDo() ? 1 : 0);
    return 1;
}

static int onRedDoMapTiledEditorLua(lua_State * lua)
{
    mbm::TILE_EDITOR * tileEditor = getTileEditorFromRawTable(lua,1,1);
    lua_pushboolean(lua,tileEditor->redDo() ? 1 : 0);
    return 1;
}

static int onClearHistoryMapTiledEditorLua(lua_State * lua)
{
    mbm::TILE_EDITOR * tileEditor = getTileEditorFromRawTable(lua,1,1);
    tileEditor->clearHistory();
    return 0;
}


static int onSetMapTypeTiledEditorLua(lua_State * lua)
{
    mbm::TILE_EDITOR * tileEditor = getTileEditorFromRawTable(lua,1,1);
    const char * mapType = luaL_checkstring(lua,2);
    tileEditor->setMapType(mapType);
    return 0;
}

static int onGetMapTypeTiledEditorLua(lua_State * lua)
{
    mbm::TILE_EDITOR * tileEditor = getTileEditorFromRawTable(lua,1,1);
    lua_pushstring(lua,tileEditor->getMapType());
    return 1;
}

static int onSetDirectionMapRenderTiledEditorLua(lua_State * lua)
{
    mbm::TILE_EDITOR * tileEditor = getTileEditorFromRawTable(lua,1,1);
    const bool left_to_right      = lua_toboolean(lua,2);
    const bool top_to_down        = lua_toboolean(lua,3);
    tileEditor->setDirectionMapRender(left_to_right,top_to_down);
    return 0;
}

static int onGetDirectionMapRenderTiledEditorLua(lua_State * lua)
{
    mbm::TILE_EDITOR * tileEditor = getTileEditorFromRawTable(lua,1,1);
    bool left_to_right      = true;
    bool top_to_down        = true;
    tileEditor->getDirectionMapRender(left_to_right,top_to_down);
    lua_pushboolean(lua,left_to_right);
    lua_pushboolean(lua,top_to_down);
    return 2;
}


static int onSetMaxTileToRenderTiledEditorLua(lua_State * lua)
{
    mbm::TILE_EDITOR * tileEditor = getTileEditorFromRawTable(lua,1,1);
    const uint32_t iMax = luaL_checkinteger(lua,2);
    tileEditor->setHowManyTile2Render(iMax);
    return 0;
}

static int onGetMaxTileToRenderTiledEditorLua(lua_State * lua)
{
    mbm::TILE_EDITOR * tileEditor = getTileEditorFromRawTable(lua,1,1);
    lua_pushinteger(lua,tileEditor->getHowManyTile2Render());
    return 1;
}


static int onMoveWholeLayerToTiledEditorLua(lua_State * lua)
{
    mbm::TILE_EDITOR * tileEditor = getTileEditorFromRawTable(lua,1,1);
    const int32_t x = luaL_checkinteger(lua,2);
    const int32_t y = luaL_checkinteger(lua,3);
    tileEditor->moveWholeLayerTo(x,y);
    return 0;
}


int onNewTileEditorLua(lua_State *lua)
{
    lua_settop(lua, 0);
    luaL_Reg regMethodsTileEditorMethods[]  = {
        // Map
        { "getMapCountWidth",            onGetMapCountWidthTiledEditorLua },
        { "setMapCountWidth",            onSetMapCountWidthTiledEditorLua },
        { "getMapCountHeight",           onGetMapCountHeightTiledEditorLua },
        { "setMapCountHeight",           onSetMapCountHeightTiledEditorLua },

        { "getMapTileWidth",             onGetMapTileWidthTiledEditorLua },
        { "setMapTileWidth",             onSetMapTileWidthTiledEditorLua },
        { "getMapTileHeight",            onGetMapTileHeightTiledEditorLua },
        { "setMapTileHeight",            onSetMapTileHeightTiledEditorLua },

        { "getMapBackgroundColor",       onGetMapBackgroundColorTiledEditorLua },
        { "setMapBackgroundColor",       onSetMapBackgroundColorTiledEditorLua },
        { "getMapBackgroundTexture",     onGetMapBackgroundTextureTiledEditorLua },
        { "setMapBackgroundTexture",     onSetMapBackgroundTextureTiledEditorLua },
        { "getMapProperties",            onGetMapPropertiesTiledEditorLua },
        { "setMapProperty",              onSetMapPropertiesTiledEditorLua },
        { "setOverBrick",                onOverBrickTiledEditorLua },
        { "selectBrick",                 onSelectBrickTiledEditorLua },
        { "unselectBrick",               onUnselectBrickTiledEditorLua },
        { "isBrickSelected",             onIsBrickSelectedTiledEditorLua },
        { "getFirstSelectedBrick",       onGetFirstSelectedBrickTiledEditorLua },
        { "getIndexTileIdOver",          onGetIndexOverBrickTiledEditorLua },
        { "getSelectedTileIDs",          onGetSelectedBrickTiledEditorLua },
        { "setBrickToLayer",             onSetBrick2LayerTiledEditorLua },
        { "rotate",                      onRotateSelectedBrickLayerTiledEditorLua },
        { "flip",                        onFlipSelectedLayerTiledEditorLua },

        { "getObjectMap",                onGetObjectMapTiledEditorLua },
        { "getTotalObjectMap",           onGetTotalObjectMapTiledEditorLua },
        { "setObjectMap",                onSetObjectMapTiledEditorLua },
        { "addObjectMap",                onAddObjectMapTiledEditorLua },
        { "deleteObjectMap",             onDeleteObjectMapTiledEditorLua },

        { "setMapType",                  onSetMapTypeTiledEditorLua },
        { "getMapType",                  onGetMapTypeTiledEditorLua },
        { "setDirectionMapRender",       onSetDirectionMapRenderTiledEditorLua },
        { "getDirectionMapRender",       onGetDirectionMapRenderTiledEditorLua },
        { "setMaxTileToRender",          onSetMaxTileToRenderTiledEditorLua },
        { "getMaxTileToRender",          onGetMaxTileToRenderTiledEditorLua },

        { "moveWholeLayerTo",            onMoveWholeLayerToTiledEditorLua },

        //History
        { "addHistoric",                onAddHistoricMapTiledEditorLua },
        { "unDo",                       onUnDoMapTiledEditorLua },
        { "redDo",                      onRedDoMapTiledEditorLua },
        { "clearHistory",               onClearHistoryMapTiledEditorLua },

        

        // Layer
        
        { "getTotalLayer",               onGetTotalLayerTiledEditorLua },
        { "getLayer",                    onGetLayerTiledEditorLua },
        { "getNameShaderLayer",          onGetNameShaderLayerTiledEditorLua },
        { "newLayer",                    onNewLayerTiledEditorLua },
        { "updateLayer",                 onUpdateLayerTiledEditorLua },
        { "deleteLayer",                 onDeleteLayerTiledEditorLua },
        { "getLayerProperties",          onGetLayerPropertiesTiledEditorLua },
        { "setLayerProperty",            onSetLayerPropertiesTiledEditorLua },
        { "selectAllBricks",             onSelectAllBricksTiledEditorLua },
        { "invertSelectedBricks",        onInvertSelectedBricksTiledEditorLua },
        { "unselectAllBricks",           onUnSelectBricksTiledEditorLua },
        { "deleteSelectedBricks",        onDeleteSelectedBricksTiledEditorLua },
        { "moveLayerUp",                 onMoveLayerUpTiledEditorLua },
        { "moveLayerDown",               onMoveLayerDownTiledEditorLua },
        { "duplicateSelectedTiles",      onDuplicateSelectedTilesLua },
                
        
        // Tile Set
        { "newTileSet",                  onNewTileSetTiledEditorLua },
        { "getTotalTileSet",             onGetTotalTileSetTiledEditorLua },
        { "getTileSetName",              onGetTileSetNameTiledEditorLua },
        { "getTileSetWidth",             onGetTileSetWidthTiledEditorLua },
        { "setTileSetWidth",             onSetTileSetWidthTiledEditorLua },
        { "getTileSetHeight",            onGetTileSetHeightTiledEditorLua },
        { "setTileSetHeight",            onSetTileSetHeightTiledEditorLua },
        { "existTileSet",                onExistTileSetTiledEditorLua },
        { "setTileSetName",              onSetTileSetNameTiledEditorLua },
        


        // Bricks
        { "getTotalBricks",              onGetTotalBricksTiledEditorLua },
        { "getBrick",                    onGetBrickTiledEditorLua },
        { "updateBrick",                 onUpdateBrickTiledEditorLua },
        { "undoChangesBrick",            onUndoChangesBrickTiledEditorLua },
        { "expandBrick",                 onExpandBrickTiledEditorLua },
        { "expandVBrick",                onExpandVBrickTiledEditorLua },
        { "expandHBrick",                onExpandHBrickTiledEditorLua },
        { "getPhysicsBrick",             onGetPhysicsBrickTiledEditorLua },
        { "setPhysicsBrick",             onSetPhysicsBrickTiledEditorLua },
        { "getBrickProperties",          onGetBrickPropertiesTiledEditorLua },
        { "getBrickID",                  onGetBrickIDByTileIDLua },
        { "setBrickProperty",            onSetBrickPropertiesTiledEditorLua },

        // Control render
        { "setRenderMode",               onSetRenderModeTiledEditorLua },
        { "showTileSetPreview",          onShowTileSetPreviewTiledEditorLua },

        // Scale
        { "getScale",                    onGetScaleTiledEditorLua },
        { "setScale",                    onSetScaleTiledEditorLua },

        { "load", onLoadTiledEditorLua },
        { "save", onSavetileEditor },
        
        { "selectTiles", onSelectTilesLua },

        //{ "isLoaded", onIsLoadedtileEditor },
        { "version", onGetVersion },
        {nullptr, nullptr}};

    luaL_newlib(lua, regMethodsTileEditorMethods);
    luaL_getmetatable(lua, "_mbmTileEditorLUA");
    lua_setmetatable(lua, -2);
    mbm::DEVICE* device       = mbm::DEVICE::getInstance();
    mbm::TILE_EDITOR **udata  = static_cast<mbm::TILE_EDITOR **>(lua_newuserdata(lua, sizeof(mbm::TILE_EDITOR*)));
    mbm::TILE_EDITOR * that   = new mbm::TILE_EDITOR(device->scene); // always 2dw
    *udata                    = that;

    /* Make our class as plugin mbm compatible to the engine. */
    luaL_getmetatable(lua,"_usertype_plugin");//are we using the module in the mbm engine?

    if(lua_type(lua,-1) == LUA_TTABLE) //Yes
    {
        lua_rawgeti(lua,-1, 1);
        PLUGIN_IDENTIFIER  = lua_tointeger(lua,-1);//update the identifier of pluging
        lua_pop(lua,1);
    }
    else
    {
        lua_pop(lua, 1);
        plugin_helper::lua_create_metatable_identifier(lua,"_usertype_plugin",PLUGIN_IDENTIFIER);//No, we just have to create a metatable to identify the module
    }
    lua_setmetatable(lua,-2);
    /* end plugin code*/

    lua_rawseti(lua, -2, 1);//set usedata as the first member in the table

    return 1;
}

int onDestroyTileEditorLua(lua_State *lua)
{
    mbm::TILE_EDITOR *      that = getTileEditorFromRawTable(lua,1,1);
#if _DEBUG
    static int v                = 1;
    printf("destroying plugin TILE_EDITOR %d \n", v++);
#endif
    delete that;
    return 0;
}

void registerClassTileEditor(lua_State *lua)
{
    luaL_Reg regMethodsTileEditorMethods[]  = {{"new", onNewTileEditorLua}, {"__gc", onDestroyTileEditorLua}, {nullptr, nullptr}};
    luaL_newmetatable(lua, "_mbmTileEditorLUA");
    luaL_setfuncs(lua, regMethodsTileEditorMethods, 0);
    lua_setglobal(lua, "TileEditor"); 
    lua_settop(lua,0);
}

//The name of this C function is the string "luaopen_" concatenated with
//   a copy of the module name where each dot is replaced by an underscore.
//Moreover, if the module name has a hyphen, its prefix up to (and including) the
//   first hyphen is removed. For instance, if the module name is a.v1-b.c, the function name will be luaopen_b_c.
//
// Note that the name of this function is not flexible
int luaopen_tilemap (lua_State *lua)
{
    registerClassTileEditor(lua);
    return onNewTileEditorLua(lua);
}


