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
   After a successful build, `copy-steam-dll.bat` runs automatically and copies the correct Steam DLL (`steam_api64.dll` or `steam_api.dll`) into the output folder (e.g. `platform-msvs\Debug\`). No manual copy is needed.
5. Place `steam_appid.txt` (containing just your App ID number, e.g. `480`) next to `mini-mbm.exe` for development runs.

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

The Steam shared library is copied automatically to the output directory as a post-build step:

- **Windows**: `copy-steam-dll.bat` runs after building the `steam` VS project — copies `steam_api64.dll` (x64) or `steam_api.dll` (Win32) to `$(OutDir)`.
- **Linux / macOS**: CMake's `POST_BUILD` step copies `libsteam_api.so` or `libsteam_api.dylib` alongside the engine binary in `bin/debug/<arch>/` or `bin/release/<arch>/`.

For **final distribution**, also include the Steam library in your shipped game folder:

| Platform | Library |
|---|---|
| Windows x64 | `redistributable_bin\win64\steam_api64.dll` |
| Windows x86 | `redistributable_bin\steam_api.dll` |
| Linux x64 | `redistributable_bin/linux64/libsteam_api.so` |
| macOS | `redistributable_bin/osx/libsteam_api.dylib` |

---

## Lua Usage

### Optional loading (recommended)

The game may run without Steam — on Android, iOS, or a PC build compiled without
`-DUSE_STEAM=1`.  Use `pcall` so a missing plugin never crashes the game:

```lua
local steamOk, steam = pcall(require, "steam")
if steamOk and steam.isReady() then
    print("Steam initialized. Player: " .. steam.getPlayerName())
else
    print("Steam not available — running without Steam features")
end
```

- `steamOk = false` — plugin DLL not present (non-Steam build, mobile, etc.) → game continues.
- `steamOk = true, steam.isReady() = false` — plugin loaded but Steam client not running → game continues.
- `steamOk = true, steam.isReady() = true` — full Steam integration active.

If you need Steam features in other scripts, expose the result as globals:

```lua
local steamOk, steam = pcall(require, "steam")
steamHandle = steamOk and steam or nil   -- nil on non-Steam builds
steamReady  = steamOk and steam.isReady() -- boolean flag other scripts can check
```

### Full load (Steam-only builds)

If the game is guaranteed to ship exclusively on Steam, a direct `require` is fine:

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

## Steam Delivery Flow

This section covers the full end-to-end path from source to a live Steam build.

### Step 1 — Build the engine with Steam support

**Visual Studio 2022:**

1. Set `STEAMWORKS_SDK_PATH` (see [Building → Windows](#windows-visual-studio-2022) above).
2. Open `platform-msvs/mini-mbm.sln`.
3. Go to **Build → Configuration Manager** and check **Build** for the **steam** project.
4. Build → **Release | Win32** (the recommended configuration for this engine).

**CMake (MinGW / Linux / macOS):**

```sh
cmake ../.. -DPLAT=Windows -DUSE_ALL=1 -DUSE_STEAM=1 \
    -DSTEAMWORKS_SDK_PATH=C:\steamworks_sdk \
    -DCMAKE_BUILD_TYPE=Release
```

### Step 2 — Package the game with `package-game.bat`

Run from `platform-msvs\`:

```bat
package-game.bat "Tower Defense Monster" "C:\Users\miche\Documents\tower-defense\assets" Release tower-defense.ico MySecretPassword
```

This produces `Tower_Defense_Monster.GameDir\` containing the executable, all DLLs
(including `steam_api.dll` for the Win32 build), and the packed `.asset` file.  
That folder is the **depot content root** — exactly what Steam will ship to players.

### Step 3 — Upload to Steam via SteamPipe

Use `upload-to-steam.bat` (in `platform-msvs\`) to generate the SteamPipe VDF
scripts and trigger the upload in one step:

```bat
upload-to-steam.bat "Tower Defense Monster" 680230 680231 ^
    "C:\Users\miche\Documents\mini-mbm\Tower_Defense_Monster.GameDir" ^
    "C:\steamcmd\steamcmd.exe" ^
    "v1.2.0 release" "beta"
```

Parameters:

| # | Parameter | Example |
|---|---|---|
| 1 | Game name | `"Tower Defense Monster"` |
| 2 | Steam App ID | `680230` |
| 3 | Steam Depot ID | `680231` *(usually App ID + 1)* |
| 4 | GameDir path | output folder from `package-game.bat` |
| 5 | `steamcmd.exe` path | `C:\steamcmd\steamcmd.exe` |
| 6 | Build description *(optional)* | `"v1.2.0 release"` |
| 7 | Branch to set live *(optional)* | `"beta"` or `"public"` |

The script:
1. Generates `depot_build_<DepotID>.vdf` — maps the GameDir tree to the depot.
2. Generates `app_build_<AppID>.vdf` — describes the build, sets the branch.
3. Calls `steamcmd +login <Steam user> +run_app_build … +quit`.
4. Prints the SteamPipe build log path when done.

> **First run**: `steamcmd` will prompt for your Steam username, password, and
> Steam Guard code.  Credentials are cached by steamcmd after the first login.

### Step 4 — Promote to live (optional)

After the build is uploaded it lands in the branch you specified (default `beta`).
To promote to the default public branch:

- **Steamworks dashboard**: *App Admin → Builds* → click **Set Build Live** on the desired build.
- **Command line** (already handled by the script when `SetLive` is `public`).

### SteamCMD installation

SteamCMD is a standalone CLI tool — separate from the Steamworks SDK.

```bat
rem Windows: create C:\steamcmd\ and extract steamcmd.zip there
curl -LO https://steamcdn-a.akamaihd.net/client/installer/steamcmd.zip
powershell Expand-Archive steamcmd.zip C:\steamcmd
```

Linux / macOS: follow <https://developer.valvesoftware.com/wiki/SteamCMD>.

---

## Troubleshooting

| Symptom | Likely cause |
|---|---|
| `require "steam"` crashes with "module not found" | Engine built without `-DUSE_STEAM=1`; use `pcall(require, "steam")` instead — see [Optional loading](#optional-loading-recommended) |
| `steam.isReady()` returns `false` | Steam client not running, or `steam_appid.txt` missing/wrong |
| Plugin fails to load | `steam_api64.dll` / `libsteam_api.so` not next to the executable |
| Build fails — SDK headers not found | `STEAMWORKS_SDK_PATH` not set, or path points to wrong directory |
| Achievements not showing | Call `steam.storeStats()` after setting achievements |
| Leaderboard callback never fires | Check that `SteamAPI_RunCallbacks()` is being called (happens automatically in `onLoop`) |
