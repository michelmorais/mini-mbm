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

#include "box2d-lua.h"
#include "physics-box-2d-lua.h"

int luaopen_box2d (lua_State *lua)
{
    lua_getfield(lua, LUA_REGISTRYINDEX, "mbm.box2d.registered");
    const bool registered = lua_toboolean(lua, -1);
    lua_pop(lua, 1);
    if (!registered)
    {
        mbm::registerClassBox2d(lua);
        lua_pushboolean(lua, 1);
        lua_setfield(lua, LUA_REGISTRYINDEX, "mbm.box2d.registered");
    }
    lua_pushboolean(lua, 1);
    return 1;
}

int luaopen_libbox2d (lua_State *lua)
{
    return luaopen_box2d(lua);
}


int mbm_open_box2d_compat(lua_State *lua)
{
#if defined(MBM_BOX2D_PARTICLES) && MBM_BOX2D_PARTICLES
    luaopen_box2d(lua);
    return 1;
#else
    return luaL_error(lua, "box2dLiquidFun requires USE_BOX2D_PARTICLES=ON (and USE_BOX2D=1, USE_LUA=1)");
#endif
}
