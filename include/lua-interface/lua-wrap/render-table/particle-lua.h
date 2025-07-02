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

#ifndef PARTICLE_2_LUA_H
#define PARTICLE_2_LUA_H

struct lua_State;

namespace util
{
    struct STAGE_PARTICLE;
};

namespace mbm
{
    class PARTICLE;
	class RENDERIZABLE;

    PARTICLE *getParticleFromRawTable(lua_State *lua, const int rawi, const int indexTable);
    util::STAGE_PARTICLE* getSelectedStageLua(lua_State *lua);
    int onDestroyParticleLua(lua_State *lua);
    int onNewIndexParticle(lua_State *lua);
    int onIndexParticle(lua_State *lua);
    int onAddParticleLua(lua_State *lua);
    int onLoadParticleLua(lua_State *lua);
    int onSetMinOffsetParticle(lua_State *lua);
    int onSetMaxOffsetParticle(lua_State *lua);
    int onGetMinOffsetParticle(lua_State *lua);
    int onGetMaxOffsetParticle(lua_State *lua);
    int onSetMinDirectionParticle(lua_State *lua);
    int onSetMaxDirectionParticle(lua_State *lua);
    int onGetMinDirectionParticle(lua_State *lua);
    int onGetMaxDirectionParticle(lua_State *lua);
    int onSetMinColorParticle(lua_State *lua);
    int onGetMinColorParticle(lua_State *lua);
    int onGetMaxColorParticle(lua_State *lua);
    int onSetMaxColorParticle(lua_State *lua);
    int onGetMinSizeParticle(lua_State *lua);
    int onSetMinSizeParticle(lua_State *lua);
    int onGetMaxSizeParticle(lua_State *lua);
    int onSetMaxSizeParticle(lua_State *lua);
    int onGetMinSpeedParticle(lua_State *lua);
    int onGetMaxSpeedParticle(lua_State *lua);
    int onSetMinSpeedParticle(lua_State *lua);
    int onSetMaxSpeedParticle(lua_State *lua);
    int onGetMinLifeTimeParticle(lua_State *lua);
    int onSetMinLifeTimeParticle(lua_State *lua);
    int onGetMaxLifeTimeParticle(lua_State *lua);
    int onSetMaxLifeTimeParticle(lua_State *lua);
    int onGetTotalAliveParticle(lua_State *lua);
    int onGetTotalParticle(lua_State *lua);
    int onGetTimeAriseParticle(lua_State *lua);
    int onSetTimeAriseParticle(lua_State *lua);
    int onGetStageTimeParticle(lua_State *lua);
    int onSetStageTimeParticle(lua_State *lua);
    int onRestartAnimParticle(lua_State *lua);
    int onAddAnimationParticle(lua_State *);
    int onSetInvertedColorParticle(lua_State *lua);
    int onGetInvertedColorParticle(lua_State *lua);
    int onGetTextureParticle(lua_State *lua);
    int onGetStageParticle(lua_State *lua);
    int onSetStageParticle(lua_State *lua);
    int onAddStageParticle(lua_State *lua);
    int onGetTotalStageParticle(lua_State *lua);
    int onNewParticleLua(lua_State *lua);
	int onNewParticleNoGcLua(lua_State *lua,RENDERIZABLE * renderizable);
    void registerClassParticle(lua_State *lua);

};

 #endif