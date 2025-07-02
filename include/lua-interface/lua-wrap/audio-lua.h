/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2015 by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                            |
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

#ifndef AUDIO_TO_LUA_H
#define AUDIO_TO_LUA_H

struct lua_State;

namespace mbm
{
    class AUDIO;
    struct USER_DATA_AUDIO_LUA;

    AUDIO *getAudioFromRawTable(lua_State *lua, const int rawi, const int indexTable);
    int onDestroyAudioLua(lua_State *lua);
    int onPlayAudio(lua_State *lua);
    int onPauseAudio(lua_State *lua);
    int onStopAudio(lua_State *lua);
    int onSetVolumeAudio(lua_State *lua);
    int onSetPanAudio(lua_State *lua);
    int onSetPitchAudio(lua_State *lua);
    int onIsPlayingAudio(lua_State *lua);
    int onIsPausedAudio(lua_State *lua);
    int onGetVolumeAudio(lua_State *lua);
    int onGetPanAudio(lua_State *lua);
    int onGetPitchAudio(lua_State *lua);
    int onResetAudio(lua_State *lua);
    int onGetLengthAudio(lua_State *lua);
    int onSetPositionAudio(lua_State *lua);
	int onForceDestroyAudioLua(lua_State *lua);
	int onGetNameAudioLua(lua_State *lua);
    void onEndStreamCallBackFromSceneThread(lua_State *lua, USER_DATA_AUDIO_LUA *userData);
    void onEndStreamCallBack(const char *fileNameStream, USER_DATA_AUDIO_LUA *userData);
    int setCallcBackAudio(lua_State *lua);
    int onNewAudioLua(lua_State *lua);
    void registerClassAudio(lua_State *lua);

};

#endif