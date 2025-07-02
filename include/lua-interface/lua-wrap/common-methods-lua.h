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

#ifndef COMMON_METHODS_LUA_H
#define COMMON_METHODS_LUA_H

#include <stdint.h>

struct lua_State;
struct luaL_Reg;

namespace mbm
{
	class RENDERIZABLE;
  int onIsOverBoundingBoxRenderizable(lua_State *lua);
  int onGetSizeRenderizableLua(lua_State *lua);
  int onGetAABBRenderizableLua(lua_State *lua);
  int onSetPosRenderizableLua(lua_State *lua);
  int onGetPosRenderizableLua(lua_State *lua);
  int onSetAngleRenderizableLua(lua_State *lua);
  int onGetAngleRenderizableLua(lua_State *lua);
  int onSetScaleRenderizableLua(lua_State *lua);
  int isOnFrustumRenderizable(lua_State *lua);
  int onDestroyRenderizable(lua_State *lua);
  int onGetScaleRenderizableLua(lua_State *lua);
  int onMoveRenderizableLua(lua_State *lua);
  int onRotateRenderizableLua(lua_State *lua);
  int onNewIndexRenderizableLua(lua_State *lua);
  int onIndexRenderizableLua(lua_State *lua);
  int onCheckCollisionBoundingBoxRenderizable(lua_State *lua);
  void doOffsetIfText(RENDERIZABLE *ptr,const float w,const float h);
  void undoOffsetIfText(RENDERIZABLE *ptr,const float w,const float h);
  void getTypeWordRenderizableLua(lua_State * lua, const int index, bool & is2dw, bool & is2ds, bool & is3d);

  class SELF_ADD_COMMON_METHODS
  {
    public:
      SELF_ADD_COMMON_METHODS(luaL_Reg ptrRegMethods[]);
      virtual ~SELF_ADD_COMMON_METHODS();
      const luaL_Reg *get(const luaL_Reg *regMethodsReplace = nullptr);
      uint32_t getSize() const;
    private:
      luaL_Reg *   regMethods;
      luaL_Reg *   prtRegAnimationsMethodsRet;
      uint32_t newSizeMethods;
  };

};

#endif
