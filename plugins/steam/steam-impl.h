/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2020 by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                            |
|                                                                                                                        |
| Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated           |
| documentation files (the "Software"), to deal in the Software without restriction, including without limitation        |
| the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and      |
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

// steam-impl.h
// Internal header shared between steam-lua.cpp and steam-class-lua.cpp.
// Do NOT include this from external code; use steam-lua.h instead.

#ifndef STEAM_IMPL_H
#define STEAM_IMPL_H

#include <core_mbm/plugin-callback.h>
#include <core_mbm/log-util.h>

#include <steam/steam_api.h>

#include <cstdlib>
#include <memory>
#include <vector>
#include <algorithm>

// Defined in steam-lua.cpp.
// True once SteamAPI_InitEx has succeeded for this process — never reset to false.
// Guards against re-initializing Steam on scene changes.
extern bool g_steamGlobalReady;

extern "C"
{
    #include <lualib.h>
    #include <lauxlib.h>
    #include <lua.h>
}

// ---------------------------------------------------------------------------
// Async leaderboard request helpers
// Each request owns its CCallResult so multiple concurrent calls are safe.
// ---------------------------------------------------------------------------

struct LB_FIND_REQ
{
    lua_State                                           *lua;
    int                                                  luaRef;
    bool                                                 done;
    CCallResult<LB_FIND_REQ, LeaderboardFindResult_t>    callResult;

    LB_FIND_REQ() noexcept : lua(nullptr), luaRef(LUA_NOREF), done(false) {}
    ~LB_FIND_REQ()
    {
        if (lua && luaRef != LUA_NOREF)
            luaL_unref(lua, LUA_REGISTRYINDEX, luaRef);
    }

    void set(lua_State *L, int ref, SteamAPICall_t call)
    {
        lua    = L;
        luaRef = ref;
        done   = false;
        callResult.Set(call, this, &LB_FIND_REQ::OnFindLeaderboard);
    }

    void OnFindLeaderboard(LeaderboardFindResult_t *pResult, bool bIOFailure)
    {
        done = true;
        if (lua && luaRef != LUA_NOREF)
        {
            lua_rawgeti(lua, LUA_REGISTRYINDEX, luaRef);
            if (!bIOFailure && pResult && pResult->m_bLeaderboardFound)
                lua_pushinteger(lua, static_cast<lua_Integer>(pResult->m_hSteamLeaderboard));
            else
                lua_pushnil(lua);

            if (lua_pcall(lua, 1, 0, 0) != LUA_OK)
            {
                ERROR_LOG("steam.findLeaderboard callback error: %s", lua_tostring(lua, -1));
                lua_pop(lua, 1);
            }
            luaL_unref(lua, LUA_REGISTRYINDEX, luaRef);
            luaRef = LUA_NOREF;
        }
    }
};

struct LB_UPLOAD_REQ
{
    lua_State                                                 *lua;
    int                                                        luaRef;
    bool                                                       done;
    CCallResult<LB_UPLOAD_REQ, LeaderboardScoreUploaded_t>    callResult;

    LB_UPLOAD_REQ() noexcept : lua(nullptr), luaRef(LUA_NOREF), done(false) {}
    ~LB_UPLOAD_REQ()
    {
        if (lua && luaRef != LUA_NOREF)
            luaL_unref(lua, LUA_REGISTRYINDEX, luaRef);
    }

    void set(lua_State *L, int ref, SteamAPICall_t call)
    {
        lua    = L;
        luaRef = ref;
        done   = false;
        callResult.Set(call, this, &LB_UPLOAD_REQ::OnUploadScore);
    }

    void OnUploadScore(LeaderboardScoreUploaded_t *pResult, bool bIOFailure)
    {
        done = true;
        if (lua && luaRef != LUA_NOREF)
        {
            lua_rawgeti(lua, LUA_REGISTRYINDEX, luaRef);
            bool success = !bIOFailure && pResult && pResult->m_bSuccess;
            lua_pushboolean(lua, success ? 1 : 0);
            if (lua_pcall(lua, 1, 0, 0) != LUA_OK)
            {
                ERROR_LOG("steam.uploadScore callback error: %s", lua_tostring(lua, -1));
                lua_pop(lua, 1);
            }
            luaL_unref(lua, LUA_REGISTRYINDEX, luaRef);
            luaRef = LUA_NOREF;
        }
    }
};

struct LB_DOWNLOAD_REQ
{
    lua_State                                                       *lua;
    int                                                              luaRef;
    bool                                                             done;
    CCallResult<LB_DOWNLOAD_REQ, LeaderboardScoresDownloaded_t>     callResult;

    LB_DOWNLOAD_REQ() noexcept : lua(nullptr), luaRef(LUA_NOREF), done(false) {}
    ~LB_DOWNLOAD_REQ()
    {
        if (lua && luaRef != LUA_NOREF)
            luaL_unref(lua, LUA_REGISTRYINDEX, luaRef);
    }

    void set(lua_State *L, int ref, SteamAPICall_t call)
    {
        lua    = L;
        luaRef = ref;
        done   = false;
        callResult.Set(call, this, &LB_DOWNLOAD_REQ::OnDownloadScores);
    }

    void OnDownloadScores(LeaderboardScoresDownloaded_t *pResult, bool bIOFailure)
    {
        done = true;
        if (!lua || luaRef == LUA_NOREF)
            return;

        lua_rawgeti(lua, LUA_REGISTRYINDEX, luaRef);

        if (!bIOFailure && pResult)
        {
            lua_newtable(lua);
            const int count = pResult->m_cEntryCount;
            for (int i = 0; i < count; ++i)
            {
                LeaderboardEntry_t entry;
                if (SteamUserStats()->GetDownloadedLeaderboardEntry(
                        pResult->m_hSteamLeaderboardEntries, i, &entry, nullptr, 0))
                {
                    lua_newtable(lua);
                    lua_pushinteger(lua, static_cast<lua_Integer>(entry.m_nGlobalRank));
                    lua_setfield(lua, -2, "rank");
                    lua_pushinteger(lua, static_cast<lua_Integer>(entry.m_nScore));
                    lua_setfield(lua, -2, "score");
                    const char *name = SteamFriends()->GetFriendPersonaName(entry.m_steamIDUser);
                    lua_pushstring(lua, name ? name : "");
                    lua_setfield(lua, -2, "name");
                    lua_rawseti(lua, -2, i + 1);
                }
            }
        }
        else
        {
            lua_pushnil(lua);
        }

        if (lua_pcall(lua, 1, 0, 0) != LUA_OK)
        {
            ERROR_LOG("steam.downloadScores callback error: %s", lua_tostring(lua, -1));
            lua_pop(lua, 1);
        }
        luaL_unref(lua, LUA_REGISTRYINDEX, luaRef);
        luaRef = LUA_NOREF;
    }
};

// ---------------------------------------------------------------------------
// STEAM_LUA — engine PLUGIN implementation
// ---------------------------------------------------------------------------

class STEAM_LUA : public PLUGIN
{
public:
    STEAM_LUA() noexcept : m_bReady(false), m_lua(nullptr) {}
    virtual ~STEAM_LUA() = default;

    // ---- PLUGIN interface --------------------------------------------------

    void onSubscribe(int /*width*/, int /*height*/, void * /*context*/, void * /*renderDevice*/) override
    {
        if (!g_steamGlobalReady)
        {
            // SteamAPI_InitEx is called at most once per process lifetime.
            // Calling it again after SteamAPI_Shutdown (or a second time without
            // shutting down) is not supported by Valve and can cause crashes.
            SteamErrMsg errMsg = {};
            ESteamAPIInitResult result = SteamAPI_InitEx(&errMsg);
            if (result == k_ESteamAPIInitResult_OK)
            {
                g_steamGlobalReady = true;
                INFO_LOG("Steam initialized successfully. App ID: %u", SteamUtils()->GetAppID());
                SteamUserStats()->RequestCurrentStats();
                // Shut down exactly once when the process exits, regardless of
                // how many scenes loaded and unloaded the Steam plugin.
                std::atexit([] { SteamAPI_Shutdown(); INFO_LOG("SteamAPI shut down."); });
            }
            else
            {
                ERROR_LOG("SteamAPI_Init failed: %s", errMsg);
                ERROR_LOG("Make sure the Steam client is running and steam_appid.txt is in the game directory.");
            }
        }
        m_bReady = g_steamGlobalReady;
    }

    void onResizeWindow(int /*width*/, int /*height*/) override {}

    void onTouchDown(int /*key*/, float /*x*/, float /*y*/) override {}
    void onTouchUp  (int /*key*/, float /*x*/, float /*y*/) override {}
    void onTouchMove(int /*key*/, float /*x*/, float /*y*/) override {}
    void onTouchZoom(float /*zoom*/) override {}

    void onKeyDown(int /*key*/) override {}
    void onKeyUp  (int /*key*/) override {}

    void onDoubleClick(float /*x*/, float /*y*/, int /*key*/) override {}

    void onKeyDownJoystick(int /*player*/, int /*key*/) override {}
    void onKeyUpJoystick  (int /*player*/, int /*key*/) override {}
    void onMoveJoystick   (int /*player*/, float /*lx*/, float /*ly*/, float /*rx*/, float /*ry*/) override {}
    void onInfoDeviceJoystick(int /*player*/, int /*maxNumberButton*/,
                              const char * /*strDeviceName*/, const char * /*extraInfo*/) override {}

    void onPrepare() override {}

    void onLoop(float /*delta*/) override
    {
        if (!m_bReady)
            return;

        SteamAPI_RunCallbacks();

        auto isDone = [](const auto &r) { return r->done; };
        m_pendingFinds.erase    (std::remove_if(m_pendingFinds.begin(),     m_pendingFinds.end(),     isDone), m_pendingFinds.end());
        m_pendingUploads.erase  (std::remove_if(m_pendingUploads.begin(),   m_pendingUploads.end(),   isDone), m_pendingUploads.end());
        m_pendingDownloads.erase(std::remove_if(m_pendingDownloads.begin(), m_pendingDownloads.end(), isDone), m_pendingDownloads.end());
    }

    void onRender() override {}

    void onDestroy() override
    {
        // Do NOT call SteamAPI_Shutdown here.
        // Steam is a process-lifetime resource: shutting it down between scenes
        // would break the Steam overlay, pending callbacks, and any subsequent
        // scene that uses require "steam". Shutdown is handled by std::atexit
        // registered on the first successful SteamAPI_InitEx call.
        m_bReady = false;
        // Clearing the vectors destroys the unique_ptrs, whose destructors
        // cancel the CCallResults and release any held Lua registry refs.
        m_pendingFinds.clear();
        m_pendingUploads.clear();
        m_pendingDownloads.clear();
    }

    // ---- Public state accessed by steam-class-lua.cpp ----------------------

    bool        m_bReady;
    lua_State  *m_lua;

    std::vector<std::unique_ptr<LB_FIND_REQ>>     m_pendingFinds;
    std::vector<std::unique_ptr<LB_UPLOAD_REQ>>   m_pendingUploads;
    std::vector<std::unique_ptr<LB_DOWNLOAD_REQ>> m_pendingDownloads;
};

// ---------------------------------------------------------------------------
// Singleton accessor — defined in steam-lua.cpp
// ---------------------------------------------------------------------------
namespace mbm
{
    STEAM_LUA *getSteamInstance() noexcept;
    void       registerClassSteam(lua_State *lua);
}

#endif // !STEAM_IMPL_H
