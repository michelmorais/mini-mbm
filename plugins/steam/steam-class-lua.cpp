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

// steam-class-lua.cpp
// Lua bindings for the Steam plugin.
//
// Usage from Lua:
//   local steam = require "steam"
//   if steam.isReady() then
//       steam.setAchievement("ACH_WIN_FIRST_GAME")
//   end
//
// All functions are registered directly on the table returned by `require "steam"`.
// They are called as steam.functionName(...), not steam:functionName(...).
// The STEAM_LUA singleton is accessed via mbm::getSteamInstance() for functions
// that need engine state (isReady, async operations).

#include "steam-lua.h"
#include "steam-impl.h"

#include <cstring>

// ---------------------------------------------------------------------------
// Helper macros
// ---------------------------------------------------------------------------
#define STEAM_CHECK_READY(lua)                                                          \
    do {                                                                                \
        STEAM_LUA *inst = mbm::getSteamInstance();                                      \
        if (!inst || !inst->m_bReady)                                                   \
        {                                                                               \
            lua_pushnil(lua);                                                           \
            return 1;                                                                   \
        }                                                                               \
    } while (0)

// ---------------------------------------------------------------------------
// Init / info
// ---------------------------------------------------------------------------

// steam.isReady() -> boolean
static int onSteamIsReadyLua(lua_State *lua)
{
    STEAM_LUA *inst = mbm::getSteamInstance();
    lua_pushboolean(lua, (inst && inst->m_bReady) ? 1 : 0);
    return 1;
}

// steam.getPlayerName() -> string
static int onSteamGetPlayerNameLua(lua_State *lua)
{
    STEAM_CHECK_READY(lua);
    const char *name = SteamFriends()->GetPersonaName();
    lua_pushstring(lua, name ? name : "");
    return 1;
}

// steam.getAppId() -> integer
static int onSteamGetAppIdLua(lua_State *lua)
{
    STEAM_CHECK_READY(lua);
    lua_pushinteger(lua, static_cast<lua_Integer>(SteamUtils()->GetAppID()));
    return 1;
}

// steam.getCurrentLanguage() -> string
static int onSteamGetCurrentLanguageLua(lua_State *lua)
{
    STEAM_CHECK_READY(lua);
    const char *lang = SteamApps()->GetCurrentGameLanguage();
    lua_pushstring(lua, lang ? lang : "english");
    return 1;
}

// ---------------------------------------------------------------------------
// Achievements
// ---------------------------------------------------------------------------

// steam.setAchievement(id: string) -> boolean
static int onSteamSetAchievementLua(lua_State *lua)
{
    STEAM_CHECK_READY(lua);
    const char *id = luaL_checkstring(lua, 1);
    bool ok = SteamUserStats()->SetAchievement(id) && SteamUserStats()->StoreStats();
    lua_pushboolean(lua, ok ? 1 : 0);
    return 1;
}

// steam.clearAchievement(id: string) -> boolean
static int onSteamClearAchievementLua(lua_State *lua)
{
    STEAM_CHECK_READY(lua);
    const char *id = luaL_checkstring(lua, 1);
    bool ok = SteamUserStats()->ClearAchievement(id) && SteamUserStats()->StoreStats();
    lua_pushboolean(lua, ok ? 1 : 0);
    return 1;
}

// steam.isAchievementAchieved(id: string) -> boolean
static int onSteamIsAchievementAchievedLua(lua_State *lua)
{
    STEAM_CHECK_READY(lua);
    const char *id = luaL_checkstring(lua, 1);
    bool achieved  = false;
    SteamUserStats()->GetAchievement(id, &achieved);
    lua_pushboolean(lua, achieved ? 1 : 0);
    return 1;
}

// ---------------------------------------------------------------------------
// Stats
// ---------------------------------------------------------------------------

// steam.setStatInt(name: string, value: integer) -> boolean
static int onSteamSetStatIntLua(lua_State *lua)
{
    STEAM_CHECK_READY(lua);
    const char *name = luaL_checkstring(lua, 1);
    int value        = static_cast<int>(luaL_checkinteger(lua, 2));
    bool ok          = SteamUserStats()->SetStat(name, value);
    lua_pushboolean(lua, ok ? 1 : 0);
    return 1;
}

// steam.setStatFloat(name: string, value: number) -> boolean
static int onSteamSetStatFloatLua(lua_State *lua)
{
    STEAM_CHECK_READY(lua);
    const char *name = luaL_checkstring(lua, 1);
    float value      = static_cast<float>(luaL_checknumber(lua, 2));
    bool ok          = SteamUserStats()->SetStat(name, value);
    lua_pushboolean(lua, ok ? 1 : 0);
    return 1;
}

// steam.getStatInt(name: string) -> integer
static int onSteamGetStatIntLua(lua_State *lua)
{
    STEAM_CHECK_READY(lua);
    const char *name = luaL_checkstring(lua, 1);
    int value        = 0;
    SteamUserStats()->GetStat(name, &value);
    lua_pushinteger(lua, static_cast<lua_Integer>(value));
    return 1;
}

// steam.getStatFloat(name: string) -> number
static int onSteamGetStatFloatLua(lua_State *lua)
{
    STEAM_CHECK_READY(lua);
    const char *name = luaL_checkstring(lua, 1);
    float value      = 0.0f;
    SteamUserStats()->GetStat(name, &value);
    lua_pushnumber(lua, static_cast<lua_Number>(value));
    return 1;
}

// steam.storeStats() -> boolean
// Flushes all pending stat/achievement changes to the Steam server.
// Call this after a sequence of setStat / setAchievement calls.
static int onSteamStoreStatsLua(lua_State *lua)
{
    STEAM_CHECK_READY(lua);
    bool ok = SteamUserStats()->StoreStats();
    lua_pushboolean(lua, ok ? 1 : 0);
    return 1;
}

// ---------------------------------------------------------------------------
// Leaderboards (async)
// ---------------------------------------------------------------------------

// steam.findLeaderboard(name: string, callback: function)
//   callback(handle: integer | nil)
//   handle is a SteamLeaderboard_t cast to integer; pass it to uploadScore /
//   downloadScores. nil indicates failure.
static int onSteamFindLeaderboardLua(lua_State *lua)
{
    STEAM_CHECK_READY(lua);
    const char *name = luaL_checkstring(lua, 1);
    luaL_checktype(lua, 2, LUA_TFUNCTION);

    SteamAPICall_t call = SteamUserStats()->FindLeaderboard(name);
    if (call == k_uAPICallInvalid)
    {
        lua_pushvalue(lua, 2);
        lua_pushnil(lua);
        lua_pcall(lua, 1, 0, 0);
        return 0;
    }

    lua_pushvalue(lua, 2);
    int ref = luaL_ref(lua, LUA_REGISTRYINDEX);

    STEAM_LUA *inst = mbm::getSteamInstance();
    auto req          = std::make_unique<LB_FIND_REQ>();
    req->set(lua, ref, call);
    inst->m_pendingFinds.push_back(std::move(req));
    return 0;
}

// steam.uploadScore(handle: integer, score: integer [, callback: function])
//   callback(success: boolean)
//   ELeaderboardUploadScoreMethod_KeepBest is used by default.
static int onSteamUploadScoreLua(lua_State *lua)
{
    STEAM_CHECK_READY(lua);
    auto handle      = static_cast<SteamLeaderboard_t>(luaL_checkinteger(lua, 1));
    int score        = static_cast<int>(luaL_checkinteger(lua, 2));
    int ref          = LUA_NOREF;
    if (lua_gettop(lua) >= 3 && lua_isfunction(lua, 3))
    {
        lua_pushvalue(lua, 3);
        ref = luaL_ref(lua, LUA_REGISTRYINDEX);
    }

    SteamAPICall_t call = SteamUserStats()->UploadLeaderboardScore(
        handle, k_ELeaderboardUploadScoreMethodKeepBest, score, nullptr, 0);

    if (call == k_uAPICallInvalid)
    {
        if (ref != LUA_NOREF)
        {
            lua_rawgeti(lua, LUA_REGISTRYINDEX, ref);
            lua_pushboolean(lua, 0);
            lua_pcall(lua, 1, 0, 0);
            luaL_unref(lua, LUA_REGISTRYINDEX, ref);
        }
        return 0;
    }

    STEAM_LUA *inst = mbm::getSteamInstance();
    auto req          = std::make_unique<LB_UPLOAD_REQ>();
    req->set(lua, ref, call);
    inst->m_pendingUploads.push_back(std::move(req));
    return 0;
}

// steam.downloadScores(handle: integer, dataType: string, startRank: integer,
//                      endRank: integer, callback: function)
//   dataType: "global" | "friends" | "around_user"
//   callback(entries: table | nil)
//   entries is an array of {rank: integer, score: integer, name: string}
static int onSteamDownloadScoresLua(lua_State *lua)
{
    STEAM_CHECK_READY(lua);
    auto handle         = static_cast<SteamLeaderboard_t>(luaL_checkinteger(lua, 1));
    const char *dtype   = luaL_checkstring(lua, 2);
    int startRank       = static_cast<int>(luaL_checkinteger(lua, 3));
    int endRank         = static_cast<int>(luaL_checkinteger(lua, 4));
    luaL_checktype(lua, 5, LUA_TFUNCTION);

    ELeaderboardDataRequest reqType = k_ELeaderboardDataRequestGlobal;
    if (strcmp(dtype, "friends") == 0)
        reqType = k_ELeaderboardDataRequestFriends;
    else if (strcmp(dtype, "around_user") == 0)
        reqType = k_ELeaderboardDataRequestGlobalAroundUser;

    SteamAPICall_t call = SteamUserStats()->DownloadLeaderboardEntries(
        handle, reqType, startRank, endRank);

    lua_pushvalue(lua, 5);
    int ref = luaL_ref(lua, LUA_REGISTRYINDEX);

    if (call == k_uAPICallInvalid)
    {
        lua_rawgeti(lua, LUA_REGISTRYINDEX, ref);
        lua_pushnil(lua);
        lua_pcall(lua, 1, 0, 0);
        luaL_unref(lua, LUA_REGISTRYINDEX, ref);
        return 0;
    }

    STEAM_LUA *inst = mbm::getSteamInstance();
    auto req          = std::make_unique<LB_DOWNLOAD_REQ>();
    req->set(lua, ref, call);
    inst->m_pendingDownloads.push_back(std::move(req));
    return 0;
}

// ---------------------------------------------------------------------------
// Cloud saves (Steam Remote Storage)
// ---------------------------------------------------------------------------

// steam.cloudWrite(filename: string, data: string) -> boolean
// Writes the Lua string (which may be binary) to Steam cloud storage.
static int onSteamCloudWriteLua(lua_State *lua)
{
    STEAM_CHECK_READY(lua);
    const char *filename = luaL_checkstring(lua, 1);
    size_t len           = 0;
    const char *data     = luaL_checklstring(lua, 2, &len);
    bool ok              = SteamRemoteStorage()->FileWrite(filename, data, static_cast<int32>(len));
    lua_pushboolean(lua, ok ? 1 : 0);
    return 1;
}

// steam.cloudRead(filename: string) -> string | nil
static int onSteamCloudReadLua(lua_State *lua)
{
    STEAM_CHECK_READY(lua);
    const char *filename = luaL_checkstring(lua, 1);
    if (!SteamRemoteStorage()->FileExists(filename))
    {
        lua_pushnil(lua);
        return 1;
    }
    int32 size = SteamRemoteStorage()->GetFileSize(filename);
    if (size <= 0)
    {
        lua_pushstring(lua, "");
        return 1;
    }
    std::vector<char> buf(static_cast<size_t>(size));
    int32 read = SteamRemoteStorage()->FileRead(filename, buf.data(), size);
    if (read > 0)
        lua_pushlstring(lua, buf.data(), static_cast<size_t>(read));
    else
        lua_pushnil(lua);
    return 1;
}

// steam.cloudDelete(filename: string) -> boolean
static int onSteamCloudDeleteLua(lua_State *lua)
{
    STEAM_CHECK_READY(lua);
    const char *filename = luaL_checkstring(lua, 1);
    bool ok              = SteamRemoteStorage()->FileDelete(filename);
    lua_pushboolean(lua, ok ? 1 : 0);
    return 1;
}

// steam.cloudFileExists(filename: string) -> boolean
static int onSteamCloudFileExistsLua(lua_State *lua)
{
    STEAM_CHECK_READY(lua);
    const char *filename = luaL_checkstring(lua, 1);
    lua_pushboolean(lua, SteamRemoteStorage()->FileExists(filename) ? 1 : 0);
    return 1;
}

// steam.isCloudEnabled() -> boolean
// Returns true only if BOTH the user account and the game have cloud saves enabled.
static int onSteamIsCloudEnabledLua(lua_State *lua)
{
    STEAM_CHECK_READY(lua);
    bool enabled = SteamRemoteStorage()->IsCloudEnabledForAccount()
                && SteamRemoteStorage()->IsCloudEnabledForApp();
    lua_pushboolean(lua, enabled ? 1 : 0);
    return 1;
}

// ---------------------------------------------------------------------------
// Overlay
// ---------------------------------------------------------------------------

// steam.activateOverlay(dialog: string)
// dialog values: "achievements", "community", "friends", "store",
//                "stats", "leaderboards", "officialgamegroup"
static int onSteamActivateOverlayLua(lua_State *lua)
{
    STEAM_CHECK_READY(lua);
    const char *dialog = luaL_checkstring(lua, 1);
    SteamFriends()->ActivateGameOverlay(dialog);
    return 0;
}

// steam.activateOverlayURL(url: string)
static int onSteamActivateOverlayURLLua(lua_State *lua)
{
    STEAM_CHECK_READY(lua);
    const char *url = luaL_checkstring(lua, 1);
    SteamFriends()->ActivateGameOverlayToWebPage(url);
    return 0;
}

// steam.showStore(appId: integer)
static int onSteamShowStoreLua(lua_State *lua)
{
    STEAM_CHECK_READY(lua);
    AppId_t appId = static_cast<AppId_t>(luaL_checkinteger(lua, 1));
    SteamFriends()->ActivateGameOverlayToStore(appId, k_EOverlayToStoreFlag_None);
    return 0;
}

// ---------------------------------------------------------------------------
// DLC
// ---------------------------------------------------------------------------

// steam.isDlcInstalled(appId: integer) -> boolean
static int onSteamIsDlcInstalledLua(lua_State *lua)
{
    STEAM_CHECK_READY(lua);
    AppId_t appId = static_cast<AppId_t>(luaL_checkinteger(lua, 1));
    lua_pushboolean(lua, SteamApps()->BIsDlcInstalled(appId) ? 1 : 0);
    return 1;
}

// ---------------------------------------------------------------------------
// registerClassSteam — builds the Lua table returned by require "steam"
// Called from onNewSteamLua (steam-lua.cpp) before the userdata is added.
// ---------------------------------------------------------------------------
namespace mbm
{
    void registerClassSteam(lua_State *lua)
    {
        luaL_Reg regMethods[] = {
            // Lifecycle / info
            {"isReady",               onSteamIsReadyLua},
            {"getPlayerName",         onSteamGetPlayerNameLua},
            {"getAppId",              onSteamGetAppIdLua},
            {"getCurrentLanguage",    onSteamGetCurrentLanguageLua},

            // Achievements
            {"setAchievement",        onSteamSetAchievementLua},
            {"clearAchievement",      onSteamClearAchievementLua},
            {"isAchievementAchieved", onSteamIsAchievementAchievedLua},

            // Stats
            {"setStatInt",            onSteamSetStatIntLua},
            {"setStatFloat",          onSteamSetStatFloatLua},
            {"getStatInt",            onSteamGetStatIntLua},
            {"getStatFloat",          onSteamGetStatFloatLua},
            {"storeStats",            onSteamStoreStatsLua},

            // Leaderboards (async)
            {"findLeaderboard",       onSteamFindLeaderboardLua},
            {"uploadScore",           onSteamUploadScoreLua},
            {"downloadScores",        onSteamDownloadScoresLua},

            // Cloud saves
            {"cloudWrite",            onSteamCloudWriteLua},
            {"cloudRead",             onSteamCloudReadLua},
            {"cloudDelete",           onSteamCloudDeleteLua},
            {"cloudFileExists",       onSteamCloudFileExistsLua},
            {"isCloudEnabled",        onSteamIsCloudEnabledLua},

            // Overlay
            {"activateOverlay",       onSteamActivateOverlayLua},
            {"activateOverlayURL",    onSteamActivateOverlayURLLua},
            {"showStore",             onSteamShowStoreLua},

            // DLC
            {"isDlcInstalled",        onSteamIsDlcInstalledLua},

            {nullptr, nullptr}
        };

        luaL_newlib(lua, regMethods);
    }
}
