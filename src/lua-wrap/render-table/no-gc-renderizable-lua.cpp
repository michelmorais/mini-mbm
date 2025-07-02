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


#include <platform/mismatch-platform.h>
#include <core_mbm/renderizable.h>
#include <lua-wrap/common-methods-lua.h>
#include <lua-wrap/user-data-lua.h>
#include <lua-wrap/check-user-type-lua.h>

#include <lua-wrap/render-table/animation-lua.h>
#include <lua-wrap/render-table/background-lua.h>
#include <lua-wrap/render-table/font-lua.h>
#include <lua-wrap/render-table/gif-view-lua.h>
#include <lua-wrap/render-table/line-mesh-lua.h>
#include <lua-wrap/render-table/mesh-lua.h>
#include <lua-wrap/render-table/particle-lua.h>
#include <lua-wrap/render-table/render-2-texture-lua.h>
#include <lua-wrap/render-table/shape-lua.h>
#include <lua-wrap/render-table/sprite-lua.h>
#include <lua-wrap/render-table/texture-view-lua.h>
#include <lua-wrap/render-table/tile-lua.h>

#ifdef USE_VR
    #include <lua-wrap/render-table/vr-lua.h>
#endif


namespace mbm
{
	int newNoGCFromRenderizable(lua_State * lua,RENDERIZABLE * renderizable)
	{
		if(renderizable == nullptr)
		{
			lua_settop(lua,0);
			lua_pushnil(lua);
			return 1;
		}

		switch (renderizable->typeClass)
		{
            case TYPE_CLASS_MESH        : return onNewMeshNoGcLua(lua,renderizable);
			case TYPE_CLASS_SPRITE      : return onNewSpriteNoGcLua(lua,renderizable);
			case TYPE_CLASS_TEXTURE     : return onNewTextureViewNoGcLua(lua,renderizable);
			case TYPE_CLASS_BACKGROUND  : return onNewBackGroundNoGcLua(lua,renderizable);
			case TYPE_CLASS_GIF         : return onNewGifViewNoGcLua(lua,renderizable);
			case TYPE_CLASS_TEXT        : { lua_pushnil(lua); return 1; }
			case TYPE_CLASS_SHAPE_MESH  : { lua_pushnil(lua); return 1; }
			case TYPE_CLASS_LINE_MESH   : { lua_pushnil(lua); return 1; }
			case TYPE_CLASS_PARTICLE    : return onNewParticleNoGcLua(lua,renderizable);
			case TYPE_CLASS_RENDER_2_TEX: { lua_pushnil(lua); return 1; }
			case TYPE_CLASS_TILE        : { lua_pushnil(lua); return 1; }
			case TYPE_CLASS_TILE_OBJ    : { lua_pushnil(lua); return 1; }
			default                     : { lua_pushnil(lua); return 1; }
		}
	}
}
