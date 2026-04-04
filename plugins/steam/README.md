# Steam Plugin for mini-mbm

Adds Steam distribution and integration to mini-mbm games via the Steamworks SDK.  
Platforms: **Windows**, **Linux**, **macOS** (not Android / iOS — Steam is a desktop platform).

---

## Prerequisites

1. **Steamworks Partner Account**: Enroll at <https://partner.steamgames.com/>. Each developer/studio uses their own App ID — this plugin never hard-codes yours.
2. **Steamworks SDK**: Download from the partner portal → *Developer Tools → Steamworks SDK*. Extract it locally (e.g. `/home/user/steamworks_sdk` or `C:\steamworks_sdk`).
3. **Steam client**: Must be running on the development machine during testing.
4. **`steam_appid.txt`**: Create a plain-text file containing your App ID (just the number, e.g. `480`). Place it in the same directory as the `mini-mbm` executable for development runs. Steam sets the App ID automatically for retail builds launched via the client.

---

## Building

### Linux / macOS (CMake)

```sh
mkdir -p build/linux_debug && cd build/linux_debug
cmake ../.. \
  -DPLAT=Linux \
  -DUSE_ALL=1 \
  -DUSE_STEAM=1 \
  -DSTEAMWORKS_SDK_PATH=/path/to/steamworks_sdk \
  -DCMAKE_BUILD_TYPE=Debug
make -j$(nproc)
```

Replace `Linux` with `MacOs` for macOS.  
Replace `Debug` with `Release` for distribution builds.

### Windows (Visual Studio 2022)

1. Open `platform-msvs/mini-mbm.sln` in Visual Studio 2022.
2. Set the `STEAMWORKS_SDK_PATH` environment variable so Visual Studio can find the SDK headers and libraries. Do this **before** opening (or after restarting) Visual Studio by one of these methods:

   **Option A — Windows Settings (recommended, permanent):**
   - Open the Start menu and search for **"Edit the system environment variables"**
   - Click **Environment Variables…**
   - Under *User variables*, click **New**
   - Name: `STEAMWORKS_SDK_PATH`  Value: `C:\steamworks_sdk` *(your actual path)*
   - Click OK on all dialogs, then **restart Visual Studio**

   **Option B — Command Prompt (current user, permanent):**
   - Open a Command Prompt (not VS Developer Prompt — a plain `cmd.exe`)
   - Run: `setx STEAMWORKS_SDK_PATH "C:\steamworks_sdk"`
   - **Restart Visual Studio** after running this — VS reads environment variables at launch, not live

   Either way, verify it worked: open a *new* Command Prompt and type `echo %STEAMWORKS_SDK_PATH%` — it should print your path.

3. The **steam** project is disabled by default in Configuration Manager (no `steam_api` headers are available without the SDK). To enable it:
   - Go to **Build → Configuration Manager**
   - Check the **Build** checkbox for the **steam** project in all desired configurations
4. Build the **steam** project.
5. Copy the Steam runtime DLL next to `mini-mbm.exe` (from your SDK root):
   - **x64**: `redistributable_bin\win64\steam_api64.dll`
   - **x86**: `redistributable_bin\steam_api.dll`
6. Place `steam_appid.txt` (containing just your App ID number, e.g. `480`) next to `mini-mbm.exe` for development runs.

### Windows (CMake + MinGW)

```bat
mkdir build\mingw_debug && cd build\mingw_debug
cmake ..\.. -G "MinGW Makefiles" ^
  -DPLAT=Windows ^
  -DUSE_ALL=1 ^
  -DUSE_STEAM=1 ^
  -DSTEAMWORKS_SDK_PATH=C:\steamworks_sdk ^
  -DCMAKE_BUILD_TYPE=Debug
mingw32-make -j%NUMBER_OF_PROCESSORS%
```

---

## Runtime Deployment

Copy the appropriate Steam shared library next to your game executable:

| Platform | Library |
|---|---|
| Windows x64 | `redistributable_bin\win64\steam_api64.dll` |
| Windows x86 | `redistributable_bin\steam_api.dll` |
| Linux x64 | `redistributable_bin\linux64\libsteam_api.so` |
| macOS | `redistributable_bin\osx\libsteam_api.dylib` |

---

## Lua Usage

```lua
-- Load the plugin from your onInitScene() callback
local steam = require "steam"

-- Check if Steam initialized correctly
if not steam.isReady() then
    print("Steam not available")
    return
end

-- Player info
local name = steam.getPlayerName()           -- -> string
local appId = steam.getAppId()               -- -> integer
local lang  = steam.getCurrentLanguage()     -- -> string (e.g. "english")

-- Achievements
steam.setAchievement("ACH_WIN_FIRST_GAME")   -- unlocks + stores immediately
steam.clearAchievement("ACH_WIN_FIRST_GAME") -- for testing only
local done = steam.isAchievementAchieved("ACH_WIN_FIRST_GAME")  -- -> boolean

-- Stats (call storeStats() to flush pending changes)
steam.setStatInt("NumWins", 42)
steam.setStatFloat("FarthestDistance", 1234.5)
local wins = steam.getStatInt("NumWins")      -- -> integer
local dist = steam.getStatFloat("FarthestDistance")  -- -> number
steam.storeStats()

-- Leaderboards (async)
steam.findLeaderboard("HighScores", function(handle)
    if handle then
        -- Upload a score (keep best)
        steam.uploadScore(handle, 9999, function(ok)
            print("Upload:", ok)
        end)
        -- Download top 10 global scores
        steam.downloadScores(handle, "global", 1, 10, function(entries)
            if entries then
                for _, e in ipairs(entries) do
                    print(e.rank, e.score, e.name)
                end
            end
        end)
    end
end)

-- Cloud saves (Steam Remote Storage)
steam.cloudWrite("save.dat", data)        -- data is a Lua string (may be binary)
local data = steam.cloudRead("save.dat")  -- -> string | nil
steam.cloudDelete("save.dat")             -- -> boolean
local exists = steam.cloudFileExists("save.dat")  -- -> boolean
local enabled = steam.isCloudEnabled()             -- -> boolean

-- Overlay
steam.activateOverlay("achievements")    -- or "friends", "community", etc.
steam.activateOverlayURL("https://steamcommunity.com/games/480")
steam.showStore(480)                     -- opens Steam store page for an App ID

-- DLC
local hasDlc = steam.isDlcInstalled(1234567)  -- -> boolean
```

---

## Security Guidance

### What is and isn't sensitive in Lua scripts

| Data | Sensitive? | Notes |
|---|---|---|
| App ID | No | Public — visible in Steam URLs |
| Achievement / stat API names | No | Defined in the Steamworks partner dashboard |
| Leaderboard names | No | Public |
| Game logic, scoring rules, level layouts | **Yes** | Protect with encryption (see below) |
| Server endpoints, internal API keys | **Yes** | Keep server-side, never in client scripts |

### Protecting Lua scripts with AES encryption

mini-mbm includes built-in AES-CBC encryption (`mbm.encrypt` / `mbm.decrypt`).  
Use the `editor/asset_packager.lua` tool to encrypt your Lua scripts before shipping:

```lua
-- Encrypt a script at build time (run from the asset packager tool):
local key = "YourSecretKey32B"  -- 16, 24, or 32 bytes
mbm.encrypt("main.lua", "main.lua.enc", key)

-- In your shipped binary, load the encrypted script:
local code = mbm.decrypt("main.lua.enc", key)
load(code)()
```

> **Tip**: Never embed the encryption key in the scripts themselves. Pass it through a secure channel (build system environment variable, native code constant, etc.).

### User authentication

Players do **not** enter Steam credentials in your game. Steam handles authentication transparently:
- The Steam client validates your App ID against the running user's license.
- `SteamAPI_InitEx` (called during `onSubscribe`) fails gracefully if the client is not running or the user does not own the game — `steam.isReady()` returns `false`.
- Your game should detect `isReady() == false` and either skip Steam features or show a "Steam not detected" message.

---

## Troubleshooting

| Symptom | Likely cause |
|---|---|
| `steam.isReady()` returns `false` | Steam client not running, or `steam_appid.txt` missing/wrong |
| Plugin fails to load | `steam_api64.dll` / `libsteam_api.so` not next to the executable |
| Build fails — SDK headers not found | `STEAMWORKS_SDK_PATH` not set, or path points to wrong directory |
| Achievements not showing | Call `steam.storeStats()` after setting achievements |
| Leaderboard callback never fires | Check that `SteamAPI_RunCallbacks()` is being called (happens automatically in `onLoop`) |
