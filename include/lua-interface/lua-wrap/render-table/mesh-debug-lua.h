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

#ifndef MESH_DEBUG_2_LUA_H
#define MESH_DEBUG_2_LUA_H

#if defined USE_EDITOR_FEATURES


struct lua_State;

namespace util
{
    struct INFO_SHADER_DATA;
};

namespace mbm
{
    struct DYNAMIC_VAR;
    class EFFECT_SHADER;
    class MESH_DEBUG_LUA;
    
    MESH_DEBUG_LUA *getMeshDebugFromRawTable(lua_State *lua, const int rawi, const int indexTable);

    int onLoadMeshDebugLua(lua_State *lua);
    int onSaveMeshDebugLua(lua_State *lua);
    int onGetInfoMeshDebugLua(lua_State *lua);
    int onGetTypeMeshDebugLua(lua_State *lua);
    int onSetPhysicsMeshDebugLua(lua_State *lua);
    int onGetPhysicsMeshDebugLua(lua_State *lua);
    int onSetTypeMeshDebugLua(lua_State *lua);
    int onGetTotalFrameMeshDebugLua(lua_State *lua);
    int onGetTotalSubsetMeshDebugLua(lua_State *lua);
    int onGetTotalVertexMeshDebugLua(lua_State *lua);
    int onGetTotalIndexMeshDebugLua(lua_State *lua);
    int onIsIndexBufferMeshDebugLua(lua_State *lua);
    int onGetVertexMeshDebugLua(lua_State *lua);
    int onSetVertexMeshDebugLua(lua_State *lua);
    int onAddVertexMeshDebugLua(lua_State *lua);
    int onGetIndexMeshDebugLua(lua_State *lua);
    int onAddIndexMeshDebugLua(lua_State *lua);
    int onGetTextureNameMeshDebugLua(lua_State *lua);
    int onSetTextureNameMeshDebugLua(lua_State *lua);
    int onAddFrameDebugLua(lua_State *lua);
    int onAddSubsetDebugLua(lua_State *lua);
    int onAddAnimationDebugLua(lua_State *lua);
    int onUpdateAnimationDebugLua(lua_State *lua);
    int onGetDetailAnimationDebugLua(lua_State *lua);
    int onCentralizeMeshDebugLua(lua_State *lua);
    int onCheckMeshDebugLua(lua_State *lua);
    int onSetStrideMeshDebugLua(lua_State *lua);
    int onEnableNormalsMeshDebugLua(lua_State *lua);
    int onEnableUvMeshDebugLua(lua_State *lua);
    void fillEffect(const EFFECT_SHADER* effect,const char* textureStage2,util::INFO_SHADER_DATA** dataInfoShader);
    int onCopyAnimationsFromMeshLua(lua_State *lua);
    int onNewIndexMeshDebug(lua_State *lua);
    int onIndexMeshDebug(lua_State *lua);
    int onNewMeshDebugLua(lua_State *lua);
    int onDestroyMeshDebugLua(lua_State *lua);
    void registerClassMeshDebug(lua_State *lua);
};

#endif

#endif