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

extern "C" 
{
    #include <lua.h>
    #include <lauxlib.h>
    #include <lualib.h>
}

#include <core_mbm/class-identifier.h>
#include <core_mbm/audio.h>
#include <core_mbm/log-util.h>
#include <lua-wrap/audio-lua.h>
#include <lua-wrap/user-data-lua.h>
#include <lua-wrap/check-user-type-lua.h>
#include <core_mbm/device.h>

namespace mbm
{
	extern int lua_error_debug(lua_State *lua, const char *format, ...);

    AUDIO *getAudioFromRawTable(lua_State *lua, const int rawi, const int indexTable)
    {
        auto **ud = static_cast<AUDIO **>(lua_check_userType(lua,rawi,indexTable,L_USER_TYPE_AUDIO));
        return *ud;
    }

	int onReleaseAudioLua(AUDIO * audio, lua_State * lua)
	{
		auto *userData = static_cast<USER_DATA_AUDIO_LUA *>(audio->userData);
		if (userData)
		{
			userData->unrefAllTableLua(lua);
			delete userData;
		}
		audio->userData = nullptr;
		audio->setOnEndstream(nullptr);
		if(audio->isPersist() == false)//we do not stop if is persistent because 'destroy' will only stop if persistent
		{
			auto audioMan = AUDIO_MANAGER::getInstance();
			audioMan->destroy(audio);
		}
		return 0;
	}

    int onDestroyAudioLua(lua_State *lua)
    {
        AUDIO *               audio   = getAudioFromRawTable(lua, 1, 1);
		#if DEBUG_FREE_LUA || defined DEBUG_AUDIO
		static int v = 1;
		PRINT_IF_DEBUG("destroying user data from audio LUA %d [%s]\n", v++, audio->fileName.c_str());
		#endif
		return onReleaseAudioLua(audio,lua);
    }

	int onForceDestroyAudioLua(lua_State *lua)
	{
		AUDIO * audio = getAudioFromRawTable(lua, 1, 1);
		#if DEBUG_FREE_LUA || defined DEBUG_AUDIO
		PRINT_IF_DEBUG("Force destroy %s\n", audio->fileName.c_str());
		#endif
		auto audio_manager = mbm::AUDIO_MANAGER::getInstance();
		audio_manager->setPersist(audio,false);
		return onReleaseAudioLua(audio, lua);
	}

    int onPlayAudio(lua_State *lua)
    {
        const int   top			= lua_gettop(lua);
        AUDIO * audio			= getAudioFromRawTable(lua, 1, 1);
        const bool  loop		= (top >= 2 ? (lua_toboolean(lua, 2) ? true : false) : false);
        if (audio->play(loop))
            lua_pushboolean(lua, 1);
        else
            lua_pushboolean(lua, 0);
        return 1;
    }

    int onPauseAudio(lua_State *lua)
    {
        AUDIO * audio = getAudioFromRawTable(lua, 1, 1);
        if(audio->pause())
			lua_pushboolean(lua, 1);
		else
			lua_pushboolean(lua, 0);
		return 1;
    }

    int onStopAudio(lua_State *lua)
    {
        AUDIO * audio = getAudioFromRawTable(lua, 1, 1);
        if(audio->stop())
			lua_pushboolean(lua, 1);
		else
			lua_pushboolean(lua, 0);
		return 1;
    }

    int onSetVolumeAudio(lua_State *lua)
    {
        AUDIO * audio = getAudioFromRawTable(lua, 1, 1);
        const float volume   = luaL_checknumber(lua, 2);
        if(audio->setVolume(volume))
			lua_pushboolean(lua, 1);
		else
			lua_pushboolean(lua, 0);
		return 1;
    }

    int onSetPanAudio(lua_State *lua)
    {
        AUDIO * audio = getAudioFromRawTable(lua, 1, 1);
        const float pan      = luaL_checknumber(lua, 2);
        if(audio->setPan(pan))
			lua_pushboolean(lua, 1);
		else
			lua_pushboolean(lua, 0);
		return 1;
    }

    int onSetPitchAudio(lua_State *lua)
    {
        AUDIO * audio = getAudioFromRawTable(lua, 1, 1);
        const float pitch    = luaL_checknumber(lua, 2);
        if(audio->setPitch(pitch))
			lua_pushboolean(lua, 1);
		else
			lua_pushboolean(lua, 0);
		return 1;
    }

    int onIsPlayingAudio(lua_State *lua)
    {
        AUDIO * audio     = getAudioFromRawTable(lua, 1, 1);
        lua_pushboolean(lua, audio->isPlaying() ? 1 : 0);
        return 1;
    }

    int onIsPausedAudio(lua_State *lua)
    {
        AUDIO * audio     = getAudioFromRawTable(lua, 1, 1);
        lua_pushboolean(lua, audio->isPaused() ? 1 : 0);
        return 1;
    }

    int onGetVolumeAudio(lua_State *lua)
    {
        AUDIO * audio     = getAudioFromRawTable(lua, 1, 1);
        lua_pushnumber(lua, audio->getVolume());
        return 1;
    }

    int onGetPanAudio(lua_State *lua)
    {
        AUDIO * audio     = getAudioFromRawTable(lua, 1, 1);
        lua_pushnumber(lua, audio->getPan());
        return 1;
    }

    int onGetPitchAudio(lua_State *lua)
    {
        AUDIO * audio     = getAudioFromRawTable(lua, 1, 1);
        lua_pushnumber(lua, audio->getPitch());
        return 1;
    }

    int onResetAudio(lua_State *lua)
    {
        AUDIO * audio    = getAudioFromRawTable(lua, 1, 1);
        if(audio->reset())
			lua_pushboolean(lua, 1);
		else
			lua_pushboolean(lua, 0);
		return 1;
    }

    int onGetLengthAudio(lua_State *lua)
    {
        AUDIO * audio     = getAudioFromRawTable(lua, 1, 1);
        lua_pushinteger(lua, audio->getLength());
        return 1;
    }

    int onSetPositionAudio(lua_State *lua)
    {
        AUDIO * audio = getAudioFromRawTable(lua, 1, 1);
        const int   pos      = luaL_checkinteger(lua, 2);
        if(audio->setPosition(pos))
			lua_pushboolean(lua, 1);
		else
			lua_pushboolean(lua, 0);
		return 1;
    }

    void onEndStreamCallBackFromSceneThread(lua_State *lua, USER_DATA_AUDIO_LUA *userData)
    {
        lua_rawgeti(lua, LUA_REGISTRYINDEX, userData->ref_CallBackStream);
        if (lua_isfunction(lua, -1))
        {
            lua_rawgeti(lua, LUA_REGISTRYINDEX, userData->ref_MeAsTable);
            lua_pushstring(lua, userData->fileNameStream.c_str());
            if (lua_pcall(lua, 2, 0, 0))
                lua_error_debug(lua, "\n%s", luaL_checkstring(lua, -1));
			lua_settop(lua,0);
        }
        else
        {
            lua_pop(lua, 1);
        }
    }

    void onEndStreamCallBack(const char *fileNameStream, USER_DATA_AUDIO_LUA *userData) // Coloca na mesma thread
    {
        if (fileNameStream)
        {
            DEVICE *        device        = DEVICE::getInstance();
            auto *userDataScene = static_cast<USER_DATA_SCENE_LUA *>(device->scene->userData);
            userDataScene->lsLuaCallBackStream.push_back(userData);
            userData->fileNameStream = fileNameStream;
        }
    }

    int setCallcBackAudio(lua_State *lua)
    {
        AUDIO *       audio    = getAudioFromRawTable(lua, 1, 1);
        auto *userData = static_cast<USER_DATA_AUDIO_LUA *>(audio->userData);
        if (audio->userData == nullptr)
        {
            audio->userData = new USER_DATA_AUDIO_LUA();
            userData        = static_cast<USER_DATA_AUDIO_LUA *>(audio->userData);
        }
        userData->refTableLua(lua, 1, &userData->ref_MeAsTable);
        userData->refFunctionLua(lua, 2, &userData->ref_CallBackStream);
        return 0;
    }

	
	int onGetNameAudioLua(lua_State *lua)
	{
		AUDIO *       audio = getAudioFromRawTable(lua, 1, 1);
		lua_pushstring(lua, audio->getFileName());
		return 1;
	}

	int onSetPersistentAudio(lua_State *lua)
	{
		AUDIO *       audio = getAudioFromRawTable(lua, 1, 1);
		bool bPersistent = lua_toboolean(lua,2);
		auto manager = AUDIO_MANAGER::getInstance();
		manager->setPersist(audio, bPersistent);
		return 0;
	}

    int onIsPersistentAudio(lua_State *lua)
	{
		AUDIO *       audio = getAudioFromRawTable(lua, 1, 1);
		const bool bPersistent = audio->isPersist();
		lua_pushboolean(lua, bPersistent ? 1 : 0);
		return 1;
	}

    int onNewAudioLua(lua_State *lua)
    {
		const int   top				= lua_gettop(lua);
		const char* fileNameSound	= luaL_checkstring(lua,2);
		const bool bInMemory		= top > 2 ? (lua_toboolean(lua,3) ? true : false) : false;
		const bool bPlay			= top > 3 ? (lua_toboolean(lua, 4) ? true : false) : false;
		const bool bLoop			= top > 4 ? (lua_toboolean(lua, 5) ? true : false) : false;
        lua_settop(lua, 0);
        /*
        **********************************
                Estado da pilha
        **********************************
        */
		auto audioMan = AUDIO_MANAGER::getInstance();
		auto audio = audioMan->load(fileNameSound, bLoop, bInMemory);
		if (audio == nullptr)
		{
			lua_pushnil(lua);
			return 1;
		}
        luaL_Reg regAudioMethods[] = {
            {"play", onPlayAudio},           {"pause", onPauseAudio},
            {"stop", onStopAudio},           {"setVolume", onSetVolumeAudio},   {"setPan", onSetPanAudio},
            {"setPitch", onSetPitchAudio},   {"isPlaying", onIsPlayingAudio},   {"isPaused", onIsPausedAudio},
            {"getVolume", onGetVolumeAudio}, {"getPan", onGetPanAudio},         {"getPitch", onGetPitchAudio},
            {"reset", onResetAudio},         {"getLen", onGetLengthAudio},      {"setPosition", onSetPositionAudio},
            {"onEnd", setCallcBackAudio},    {"destroy",onForceDestroyAudioLua},{ "getName",onGetNameAudioLua },
			{ "setPersistent", onSetPersistentAudio },							{ "isPersistent", onIsPersistentAudio },
            {nullptr, nullptr}};
        luaL_newlib(lua, regAudioMethods);
        /*
        **********************************
                Estado da pilha
                -1|    table |1
        **********************************
        */
        luaL_getmetatable(lua, "_mbmAudio");
        /*
        **********************************
                Estado da pilha
                -2|    table |1
                -1|    table |2
        **********************************
        */
        lua_setmetatable(lua, -2);
        /*
        **********************************
                Estado da pilha
                -1|    table |1
        **********************************
        */
        auto **udata = static_cast<AUDIO **>(lua_newuserdata(lua, sizeof(AUDIO *)));
        /*
        **********************************
                Estado da pilha
                -2|    table |1
                -1| userdata |2
        **********************************
        */
		
        *udata           = audio;

        /* trick to ensure that we will receive the expected metatable type expected metatable type. */
        const char* __userdata_name = getUserTypeAsString(L_USER_TYPE_AUDIO);
        luaL_getmetatable(lua,__userdata_name);
        lua_setmetatable(lua,-2);
        /* end trick */

        lua_rawseti(lua, -2, 1);
        /*
        **********************************
                Estado da pilha
                -1|    table |1
        **********************************
        */
		if(bPlay)
			audio->play(bLoop);
        return 1;
    }

    void registerClassAudio(lua_State *lua)
    {
        luaL_Reg regAudioMethods[] = {{"new", onNewAudioLua}, {"__gc", onDestroyAudioLua}, {nullptr, nullptr}};
        luaL_newmetatable(lua, "_mbmAudio");
        luaL_setfuncs(lua, regAudioMethods, 0);
        lua_setglobal(lua, "audio");
        lua_settop(lua,0);
    }
};
