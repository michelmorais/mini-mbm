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

#ifndef TILE_2_LUA_H
#define TILE_2_LUA_H

#include <vector>
#include <string>
#include <core_mbm/header-mesh.h>


struct lua_State;

namespace mbm
{
    class TILE;

	struct TILE_FILTER
    {
        std::string name;
        std::string owner;
        std::string type;
        std::string ID;
        TILE_FILTER() = default;

        bool belongID(const uint16_t _ID)const;
        bool belong(const util::BTILE_PROPERTY* property)const;
        bool belong(const util::BTILE_OBJ * bObj)const;
        bool belong(const std::string & item,const std::string & to_find)const;
        bool belong(const util::BTILE_OBJ_TYPE item,const std::string & to_find)const;
        bool belong(const util::BTILE_PROPERTY_TYPE item,const std::string & to_find)const;
    };
	
	TILE *getTileFromRawTable(lua_State *lua, const int rawi, const int indexTable);
	int onDestroyTileLua(lua_State *lua);
	int onDestroyObjTileLua(lua_State *lua);
    int onLoadTileLua(lua_State *lua);
	int onRenderAllTile(lua_State *lua);
	int onSetLayerVisibleLuaTile(lua_State *lua);
	int onGetTotalLayerLuaTile(lua_State *lua);
	int onSetStepLayerLuaTile(lua_State *lua);
	int onGetStepLayerLuaTile(lua_State *lua);
	int onNewTileLua(lua_State *lua);
    void registerClassTile(lua_State *lua);
};

#endif